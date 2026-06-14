#include "ea_tool_process.h"

#include <ea_tool_args.h>
#include <ea_tool_registry.h>
#include <ev_logger.h>

#include <dirent.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace embedagent {
namespace example {

// ---------------------------------------------------------------------------
// Internal types and helpers
// ---------------------------------------------------------------------------

namespace {

struct ProcInfo {
    int         pid;
    std::string name;
    char        state;
    long        vmRssKb;   // resident set size in KB
    long        vmVirtKb;  // virtual memory in KB
    int         numThreads;
    std::string user;      // resolved via /proc/{pid}/status Uid field + getpwuid
    std::string cmdline;
    long        utime;     // user-mode CPU ticks
    long        stime;     // kernel-mode CPU ticks
};

// Escape a JSON string value.
static std::string jsonStr(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '"' || s[i] == '\\') { out += '\\'; }
        else if (s[i] == '\n' || s[i] == '\r') { out += ' '; continue; }
        out += s[i];
    }
    out += '"';
    return out;
}

// Parse /proc/{pid}/status for Name, State, VmRSS, VmSize, Threads, Uid.
static bool parseProcStatus(int pid, ProcInfo* info) {
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%d/status", pid);
    std::ifstream f(path);
    if (!f.is_open()) { return false; }

    std::string line;
    while (std::getline(f, line)) {
        if (line.compare(0, 5, "Name:") == 0) {
            std::istringstream ss(line.substr(5));
            ss >> info->name;
        } else if (line.compare(0, 6, "State:") == 0) {
            std::istringstream ss(line.substr(6));
            ss >> info->state;
        } else if (line.compare(0, 6, "VmRSS:") == 0) {
            std::istringstream ss(line.substr(6));
            ss >> info->vmRssKb;
        } else if (line.compare(0, 7, "VmSize:") == 0) {
            std::istringstream ss(line.substr(7));
            ss >> info->vmVirtKb;
        } else if (line.compare(0, 8, "Threads:") == 0) {
            std::istringstream ss(line.substr(8));
            ss >> info->numThreads;
        } else if (line.compare(0, 4, "Uid:") == 0) {
            // Real UID is the first field.
            unsigned int uid = 0;
            std::istringstream ss(line.substr(4));
            ss >> uid;
            // Resolve UID to name via /etc/passwd (simple scan, no getpwuid_r dependency).
            std::ifstream pw("/etc/passwd");
            std::string pwline;
            while (std::getline(pw, pwline)) {
                // format: name:x:uid:gid:...
                std::size_t c1 = pwline.find(':');
                if (c1 == std::string::npos) { continue; }
                std::size_t c2 = pwline.find(':', c1 + 1);
                if (c2 == std::string::npos) { continue; }
                std::size_t c3 = pwline.find(':', c2 + 1);
                if (c3 == std::string::npos) { continue; }
                unsigned int pwuid = 0;
                std::istringstream uidss(pwline.substr(c2 + 1, c3 - c2 - 1));
                uidss >> pwuid;
                if (pwuid == uid) {
                    info->user = pwline.substr(0, c1);
                    break;
                }
            }
            if (info->user.empty()) {
                char ubuf[16];
                std::snprintf(ubuf, sizeof(ubuf), "%u", uid);
                info->user = ubuf;
            }
        }
    }
    return !info->name.empty();
}

// Parse CPU ticks from /proc/{pid}/stat.
static void parseProcStat(int pid, long* utime, long* stime) {
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    std::ifstream f(path);
    if (!f.is_open()) { return; }

    std::string line;
    std::getline(f, line);
    // Format: pid (name) state ppid pgroup ... utime(14) stime(15) ...
    // Skip past the closing ')' of the comm field.
    std::size_t rparen = line.rfind(')');
    if (rparen == std::string::npos) { return; }

    std::istringstream ss(line.substr(rparen + 2));
    std::string state, ppid, pgrp, session, tty, tpgid, flags,
                minflt, cminflt, majflt, cmajflt;
    ss >> state >> ppid >> pgrp >> session >> tty >> tpgid >> flags
       >> minflt >> cminflt >> majflt >> cmajflt >> *utime >> *stime;
}

// Read /proc/{pid}/cmdline (NUL-separated args → space-separated string).
static std::string readCmdline(int pid) {
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) { return ""; }

    std::string raw;
    std::getline(f, raw, '\0');  // read first arg only to avoid huge strings
    if (raw.empty()) { return ""; }

    // Replace remaining NUL bytes with spaces (up to 256 chars).
    std::string full(raw);
    if (full.size() > 256) { full.resize(256); }
    for (std::size_t i = 0; i < full.size(); ++i) {
        if (full[i] == '\0') { full[i] = ' '; }
    }
    return full;
}

// Scan /proc for all numeric directory entries (PIDs).
static std::vector<int> listPids() {
    std::vector<int> pids;
    DIR* dir = ::opendir("/proc");
    if (!dir) { return pids; }
    struct dirent* e;
    while ((e = ::readdir(dir)) != NULL) {
        int pid = 0;
        bool allDigit = true;
        for (const char* p = e->d_name; *p; ++p) {
            if (*p < '0' || *p > '9') { allDigit = false; break; }
            pid = pid * 10 + (*p - '0');
        }
        if (allDigit && pid > 0) { pids.push_back(pid); }
    }
    ::closedir(dir);
    return pids;
}

static bool cmpByRss(const ProcInfo& a, const ProcInfo& b) {
    return a.vmRssKb > b.vmRssKb;
}

}  // namespace

// ---------------------------------------------------------------------------
// Tool handlers
// ---------------------------------------------------------------------------

static bool handleListTopProcesses(const EaToolArgs& args, std::string* out) {
    int count = args.getInt("count", 5);
    if (count <= 0) { count = 5; }
    if (count > 32) { count = 32; }
    LOGI("ProcessTool: list_top_processes(count=%d)", count);

    std::vector<int> pids = listPids();
    std::vector<ProcInfo> infos;
    infos.reserve(pids.size());

    for (std::size_t i = 0; i < pids.size(); ++i) {
        ProcInfo info;
        info.pid        = pids[i];
        info.state      = '?';
        info.vmRssKb    = 0;
        info.vmVirtKb   = 0;
        info.numThreads = 0;
        info.utime      = 0;
        info.stime      = 0;
        if (!parseProcStatus(pids[i], &info)) { continue; }
        parseProcStat(pids[i], &info.utime, &info.stime);
        info.cmdline = readCmdline(pids[i]);
        infos.push_back(info);
    }

    std::sort(infos.begin(), infos.end(), cmpByRss);

    int limit = count < static_cast<int>(infos.size())
                ? count : static_cast<int>(infos.size());

    std::string json = "[";
    for (int i = 0; i < limit; ++i) {
        if (i > 0) { json += ','; }
        const ProcInfo& p = infos[static_cast<std::size_t>(i)];
        char entry[512];
        std::snprintf(entry, sizeof(entry),
            "{\"pid\":%d,\"name\":%s,\"state\":\"%c\","
            "\"rss_kb\":%ld,\"virt_kb\":%ld,"
            "\"threads\":%d,\"user\":%s}",
            p.pid, jsonStr(p.name).c_str(), p.state,
            p.vmRssKb, p.vmVirtKb,
            p.numThreads, jsonStr(p.user).c_str());
        json += entry;
    }
    json += ']';
    *out = json;
    return true;
}

static bool handleGetProcessInfo(const EaToolArgs& args, std::string* out) {
    int pid = args.getInt("pid", -1);
    LOGI("ProcessTool: get_process_info(pid=%d)", pid);

    if (pid <= 0) {
        *out = "{\"error\":\"invalid pid\"}";
        return true;
    }

    ProcInfo info;
    info.pid        = pid;
    info.state      = '?';
    info.vmRssKb    = 0;
    info.vmVirtKb   = 0;
    info.numThreads = 0;
    info.utime      = 0;
    info.stime      = 0;

    if (!parseProcStatus(pid, &info)) {
        char buf[64];
        std::snprintf(buf, sizeof(buf),
            "{\"error\":\"process %d not found\"}", pid);
        *out = buf;
        return true;
    }

    parseProcStat(pid, &info.utime, &info.stime);
    info.cmdline = readCmdline(pid);

    long clkTck = ::sysconf(_SC_CLK_TCK);
    if (clkTck <= 0) { clkTck = 100; }
    double cpuSec = static_cast<double>(info.utime + info.stime) /
                    static_cast<double>(clkTck);

    char buf[1024];
    std::snprintf(buf, sizeof(buf),
        "{\"pid\":%d,\"name\":%s,\"state\":\"%c\","
        "\"rss_kb\":%ld,\"virt_kb\":%ld,"
        "\"threads\":%d,\"user\":%s,"
        "\"cpu_sec\":%.2f,\"cmdline\":%s}",
        info.pid, jsonStr(info.name).c_str(), info.state,
        info.vmRssKb, info.vmVirtKb,
        info.numThreads, jsonStr(info.user).c_str(),
        cpuSec, jsonStr(info.cmdline).c_str());
    *out = buf;
    return true;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void registerProcessTools(EaToolRegistry* registry) {
    std::vector<EaToolParam> topParams;
    {
        EaToolParam p;
        p.name        = "count";
        p.type        = EaParamType::kInteger;
        p.description = "Number of processes to return (1-32, default 5), "
                        "sorted by resident memory usage (RSS) descending";
        p.required    = false;
        topParams.push_back(p);
    }
    registry->registerTool(
        "list_top_processes",
        "List the top N processes on the system sorted by resident memory "
        "usage (RSS). Returns PID, name, state, RSS/virtual memory (KB), "
        "thread count, and owner.",
        topParams,
        handleListTopProcesses);

    std::vector<EaToolParam> infoParams;
    {
        EaToolParam p;
        p.name        = "pid";
        p.type        = EaParamType::kInteger;
        p.description = "Process ID to inspect";
        p.required    = true;
        infoParams.push_back(p);
    }
    registry->registerTool(
        "get_process_info",
        "Return detailed information for a specific process: name, state "
        "(R/S/D/Z/T), RSS and virtual memory (KB), thread count, owner, "
        "cumulative CPU time (seconds), and full command line.",
        infoParams,
        handleGetProcessInfo);

    LOGI("ProcessTool: registered list_top_processes, get_process_info");
}

}  // namespace example
}  // namespace embedagent

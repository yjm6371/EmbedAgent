#include "ea_tool_sysinfo.h"

#include <ea_tool_args.h>
#include <ea_tool_registry.h>
#include <ev_logger.h>

#include <sys/statvfs.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace embedagent {
namespace example {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

// Escape a JSON string value (backslash and double-quote only).
static std::string jsonStr(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '"' || s[i] == '\\') { out += '\\'; }
        out += s[i];
    }
    out += '"';
    return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// Tool handlers
// ---------------------------------------------------------------------------

static bool handleGetSystemInfo(const EaToolArgs& /*args*/, std::string* out) {
    LOGI("SysinfoTool: get_system_info");

    // /proc/uptime: <uptime_seconds> <idle_seconds>
    double uptimeSec = 0.0;
    double idleSec   = 0.0;
    {
        std::ifstream f("/proc/uptime");
        if (f.is_open()) {
            f >> uptimeSec >> idleSec;
        }
    }

    // /proc/loadavg: <1min> <5min> <15min> <running/total> <last_pid>
    double load1 = 0.0;
    double load5 = 0.0;
    double load15 = 0.0;
    int    runningProcs = 0;
    int    totalProcs   = 0;
    {
        std::ifstream f("/proc/loadavg");
        if (f.is_open()) {
            char slash = '/';
            f >> load1 >> load5 >> load15 >> runningProcs >> slash >> totalProcs;
        }
    }

    long uptimeMin  = static_cast<long>(uptimeSec) / 60;
    long uptimeHour = uptimeMin / 60;
    uptimeMin       = uptimeMin % 60;

    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "{\"uptime_sec\":%ld,\"uptime_h\":%ld,\"uptime_m\":%ld,"
        "\"load_1min\":%.2f,\"load_5min\":%.2f,\"load_15min\":%.2f,"
        "\"running_procs\":%d,\"total_procs\":%d}",
        static_cast<long>(uptimeSec), uptimeHour, uptimeMin,
        load1, load5, load15, runningProcs, totalProcs);
    *out = buf;
    return true;
}

static bool handleGetMemoryInfo(const EaToolArgs& /*args*/, std::string* out) {
    LOGI("SysinfoTool: get_memory_info");

    long totalKb     = 0;
    long freeKb      = 0;
    long availableKb = 0;
    long buffersKb   = 0;
    long cachedKb    = 0;
    long swapTotalKb = 0;
    long swapFreeKb  = 0;

    std::ifstream f("/proc/meminfo");
    if (f.is_open()) {
        std::string key;
        long        value = 0;
        std::string unit;
        while (f >> key >> value) {
            std::getline(f, unit); // consume " kB\n"
            if      (key == "MemTotal:")     { totalKb     = value; }
            else if (key == "MemFree:")      { freeKb      = value; }
            else if (key == "MemAvailable:") { availableKb = value; }
            else if (key == "Buffers:")      { buffersKb   = value; }
            else if (key == "Cached:")       { cachedKb    = value; }
            else if (key == "SwapTotal:")    { swapTotalKb = value; }
            else if (key == "SwapFree:")     { swapFreeKb  = value; }
        }
    }

    long usedKb = totalKb - freeKb - buffersKb - cachedKb;
    if (usedKb < 0) { usedKb = totalKb - freeKb; }

    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "{\"total_mb\":%ld,\"used_mb\":%ld,\"free_mb\":%ld,"
        "\"available_mb\":%ld,\"buffers_mb\":%ld,\"cached_mb\":%ld,"
        "\"swap_total_mb\":%ld,\"swap_free_mb\":%ld,"
        "\"used_percent\":%ld}",
        totalKb / 1024, usedKb / 1024, freeKb / 1024,
        availableKb / 1024, buffersKb / 1024, cachedKb / 1024,
        swapTotalKb / 1024, swapFreeKb / 1024,
        totalKb > 0 ? (usedKb * 100 / totalKb) : 0L);
    *out = buf;
    return true;
}

static bool handleGetDiskUsage(const EaToolArgs& args, std::string* out) {
    std::string path = args.getString("path", "/");
    LOGI("SysinfoTool: get_disk_usage(path=%s)", path.c_str());

    struct statvfs st;
    if (::statvfs(path.c_str(), &st) != 0) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "{\"error\":\"statvfs failed: %s\",\"path\":%s}",
            std::strerror(errno), jsonStr(path).c_str());
        *out = buf;
        return true;
    }

    unsigned long long blockSize  = st.f_frsize ? st.f_frsize : st.f_bsize;
    unsigned long long totalBytes = blockSize * st.f_blocks;
    unsigned long long freeBytes  = blockSize * st.f_bfree;
    unsigned long long usedBytes  = totalBytes - freeBytes;
    unsigned long long availBytes = blockSize * st.f_bavail;
    int usedPercent = (totalBytes > 0)
        ? static_cast<int>(usedBytes * 100 / totalBytes)
        : 0;

    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "{\"path\":%s,\"total_gb\":%.2f,\"used_gb\":%.2f,"
        "\"free_gb\":%.2f,\"available_gb\":%.2f,\"used_percent\":%d}",
        jsonStr(path).c_str(),
        static_cast<double>(totalBytes) / (1024.0 * 1024 * 1024),
        static_cast<double>(usedBytes)  / (1024.0 * 1024 * 1024),
        static_cast<double>(freeBytes)  / (1024.0 * 1024 * 1024),
        static_cast<double>(availBytes) / (1024.0 * 1024 * 1024),
        usedPercent);
    *out = buf;
    return true;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void registerSysinfoTools(EaToolRegistry* registry) {
    registry->registerTool(
        "get_system_info",
        "Return the system uptime (seconds, hours, minutes) and CPU load averages "
        "for the past 1, 5, and 15 minutes, plus running/total process counts.",
        {},
        handleGetSystemInfo);

    registry->registerTool(
        "get_memory_info",
        "Return a breakdown of physical RAM and swap usage: total, used, free, "
        "available, buffers, cached (all in MB) and used percentage.",
        {},
        handleGetMemoryInfo);

    std::vector<EaToolParam> diskParams;
    {
        EaToolParam p;
        p.name        = "path";
        p.type        = EaParamType::kString;
        p.description = "Filesystem mount point or directory path to inspect "
                        "(e.g. \"/\", \"/home\", \"/var\")";
        p.required    = false;
        diskParams.push_back(p);
    }
    registry->registerTool(
        "get_disk_usage",
        "Return filesystem usage statistics for a given path: "
        "total, used, free, and available space in GB plus used percentage.",
        diskParams,
        handleGetDiskUsage);

    LOGI("SysinfoTool: registered get_system_info, get_memory_info, get_disk_usage");
}

}  // namespace example
}  // namespace embedagent

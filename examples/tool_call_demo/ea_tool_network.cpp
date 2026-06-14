#include "ea_tool_network.h"

#include <ea_tool_args.h>
#include <ea_tool_registry.h>
#include <ev_logger.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace embedagent {
namespace example {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

// Escape a JSON string value.
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

static std::string readSysNetFile(const std::string& iface,
                                  const std::string& filename) {
    std::string path = "/sys/class/net/" + iface + "/" + filename;
    std::ifstream f(path.c_str());
    std::string val;
    if (f.is_open()) { f >> val; }
    return val;
}

// Get IPv4 address for an interface using ioctl.
static std::string getIPv4(const std::string& iface) {
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { return ""; }

    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    std::strncpy(ifr.ifr_name, iface.c_str(), IFNAMSIZ - 1);

    std::string ip;
    if (::ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
        char buf[INET_ADDRSTRLEN] = {};
        struct sockaddr_in* sa = reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_addr);
        if (::inet_ntop(AF_INET, &sa->sin_addr, buf, sizeof(buf)) != NULL) {
            ip = buf;
        }
    }
    ::close(fd);
    return ip;
}

// Get netmask for an interface.
static std::string getNetmask(const std::string& iface) {
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { return ""; }

    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    std::strncpy(ifr.ifr_name, iface.c_str(), IFNAMSIZ - 1);

    std::string mask;
    if (::ioctl(fd, SIOCGIFNETMASK, &ifr) == 0) {
        char buf[INET_ADDRSTRLEN] = {};
        struct sockaddr_in* sa = reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_netmask);
        if (::inet_ntop(AF_INET, &sa->sin_addr, buf, sizeof(buf)) != NULL) {
            mask = buf;
        }
    }
    ::close(fd);
    return mask;
}

// Enumerate interface names from /sys/class/net/.
static std::vector<std::string> listInterfaces() {
    std::vector<std::string> ifaces;
    DIR* dir = ::opendir("/sys/class/net");
    if (!dir) { return ifaces; }
    struct dirent* e;
    while ((e = ::readdir(dir)) != NULL) {
        const std::string name(e->d_name);
        if (name == "." || name == "..") { continue; }
        ifaces.push_back(name);
    }
    ::closedir(dir);
    return ifaces;
}

// Read a single unsigned long long from /sys/class/net/{iface}/statistics/{file}.
static unsigned long long readStatUll(const std::string& iface,
                                      const std::string& stat) {
    std::string path = "/sys/class/net/" + iface + "/statistics/" + stat;
    std::ifstream f(path.c_str());
    unsigned long long val = 0;
    if (f.is_open()) { f >> val; }
    return val;
}

}  // namespace

// ---------------------------------------------------------------------------
// Tool handlers
// ---------------------------------------------------------------------------

static bool handleGetNetworkInterfaces(const EaToolArgs& /*args*/, std::string* out) {
    LOGI("NetworkTool: get_network_interfaces");

    std::vector<std::string> ifaces = listInterfaces();
    std::string json = "[";
    bool first = true;

    for (std::size_t i = 0; i < ifaces.size(); ++i) {
        const std::string& name = ifaces[i];

        std::string mac       = readSysNetFile(name, "address");
        std::string operState = readSysNetFile(name, "operstate");
        std::string mtu       = readSysNetFile(name, "mtu");
        std::string ip        = getIPv4(name);
        std::string mask      = getNetmask(name);

        if (!first) { json += ','; }
        first = false;

        char entry[512];
        std::snprintf(entry, sizeof(entry),
            "{\"name\":%s,\"mac\":%s,\"ip\":%s,\"netmask\":%s,"
            "\"state\":%s,\"mtu\":%s}",
            jsonStr(name).c_str(),
            jsonStr(mac.empty()       ? "unknown" : mac).c_str(),
            jsonStr(ip.empty()        ? ""        : ip).c_str(),
            jsonStr(mask.empty()      ? ""        : mask).c_str(),
            jsonStr(operState.empty() ? "unknown" : operState).c_str(),
            jsonStr(mtu.empty()       ? "0"       : mtu).c_str());
        json += entry;
    }
    json += ']';
    *out = json;
    return true;
}

static bool handleGetInterfaceStats(const EaToolArgs& args, std::string* out) {
    std::string iface = args.getString("iface", "");
    LOGI("NetworkTool: get_interface_stats(iface=%s)", iface.c_str());

    if (iface.empty()) {
        *out = "{\"error\":\"iface parameter is required\"}";
        return true;
    }

    // Verify the interface exists.
    std::string operState = readSysNetFile(iface, "operstate");
    if (operState.empty()) {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
            "{\"error\":\"interface %s not found\"}", iface.c_str());
        *out = buf;
        return true;
    }

    unsigned long long rxBytes   = readStatUll(iface, "rx_bytes");
    unsigned long long rxPackets = readStatUll(iface, "rx_packets");
    unsigned long long rxErrors  = readStatUll(iface, "rx_errors");
    unsigned long long rxDropped = readStatUll(iface, "rx_dropped");
    unsigned long long txBytes   = readStatUll(iface, "tx_bytes");
    unsigned long long txPackets = readStatUll(iface, "tx_packets");
    unsigned long long txErrors  = readStatUll(iface, "tx_errors");
    unsigned long long txDropped = readStatUll(iface, "tx_dropped");

    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "{\"iface\":%s,"
        "\"rx_bytes\":%llu,\"rx_packets\":%llu,"
        "\"rx_errors\":%llu,\"rx_dropped\":%llu,"
        "\"tx_bytes\":%llu,\"tx_packets\":%llu,"
        "\"tx_errors\":%llu,\"tx_dropped\":%llu}",
        jsonStr(iface).c_str(),
        rxBytes, rxPackets, rxErrors, rxDropped,
        txBytes, txPackets, txErrors, txDropped);
    *out = buf;
    return true;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void registerNetworkTools(EaToolRegistry* registry) {
    registry->registerTool(
        "get_network_interfaces",
        "List all network interfaces on the device. For each interface, "
        "returns the name, MAC address, IPv4 address, netmask, "
        "operational state (up/down/unknown), and MTU.",
        {},
        handleGetNetworkInterfaces);

    std::vector<EaToolParam> statsParams;
    {
        EaToolParam p;
        p.name        = "iface";
        p.type        = EaParamType::kString;
        p.description = "Network interface name (e.g. \"eth0\", \"wlan0\")";
        p.required    = true;
        statsParams.push_back(p);
    }
    registry->registerTool(
        "get_interface_stats",
        "Return RX/TX traffic statistics for a single network interface: "
        "bytes, packets, errors, and dropped counts in each direction.",
        statsParams,
        handleGetInterfaceStats);

    LOGI("NetworkTool: registered get_network_interfaces, get_interface_stats");
}

}  // namespace example
}  // namespace embedagent

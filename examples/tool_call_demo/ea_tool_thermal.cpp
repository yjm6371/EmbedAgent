#include "ea_tool_thermal.h"

#include <ea_tool_args.h>
#include <ea_tool_registry.h>
#include <ev_logger.h>

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

// Read millidegree Celsius from a thermal sysfs temp file.
// Returns false and leaves *millic unchanged on error.
static bool readThermalMilliC(const char* path, long* millic) {
    std::ifstream f(path);
    if (!f.is_open()) { return false; }
    return static_cast<bool>(f >> *millic);
}

// Read the type string of a thermal zone (e.g. "cpu-thermal", "x86_pkg_temp").
static std::string readThermalType(const std::string& zoneDir) {
    std::ifstream f(zoneDir + "/type");
    std::string type;
    if (f.is_open()) { f >> type; }
    return type.empty() ? "unknown" : type;
}

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

struct ThermalZone {
    std::string name;
    std::string type;
    double      tempC;
};

// Scan /sys/class/thermal/ for all thermal_zone* directories.
static std::vector<ThermalZone> scanThermalZones() {
    std::vector<ThermalZone> zones;

    DIR* dir = ::opendir("/sys/class/thermal");
    if (!dir) { return zones; }

    struct dirent* entry;
    while ((entry = ::readdir(dir)) != NULL) {
        const std::string name(entry->d_name);
        if (name.compare(0, 12, "thermal_zone") != 0) { continue; }

        std::string zoneDir = std::string("/sys/class/thermal/") + name;
        std::string tempPath = zoneDir + "/temp";

        long millic = 0;
        if (!readThermalMilliC(tempPath.c_str(), &millic)) { continue; }

        ThermalZone z;
        z.name  = name;
        z.type  = readThermalType(zoneDir);
        z.tempC = static_cast<double>(millic) / 1000.0;
        zones.push_back(z);
    }
    ::closedir(dir);
    return zones;
}

}  // namespace

// ---------------------------------------------------------------------------
// Tool handlers
// ---------------------------------------------------------------------------

static bool handleReadCpuTemperature(const EaToolArgs& /*args*/, std::string* out) {
    LOGI("ThermalTool: read_cpu_temperature");

    long millic = 0;
    bool ok = readThermalMilliC("/sys/class/thermal/thermal_zone0/temp", &millic);

    if (!ok) {
        // Fallback: scan all zones for the first one of a CPU-related type.
        std::vector<ThermalZone> zones = scanThermalZones();
        for (std::size_t i = 0; i < zones.size(); ++i) {
            const std::string& t = zones[i].type;
            if (t.find("cpu") != std::string::npos ||
                t.find("x86_pkg") != std::string::npos ||
                t.find("soc") != std::string::npos) {
                millic = static_cast<long>(zones[i].tempC * 1000);
                ok = true;
                break;
            }
        }
    }

    if (!ok) {
        *out = "{\"error\":\"no thermal zone available\"}";
        return true;
    }

    double tempC = static_cast<double>(millic) / 1000.0;
    char buf[128];
    std::snprintf(buf, sizeof(buf),
        "{\"temp_c\":%.1f,\"temp_f\":%.1f}",
        tempC, tempC * 9.0 / 5.0 + 32.0);
    *out = buf;
    return true;
}

static bool handleListThermalZones(const EaToolArgs& /*args*/, std::string* out) {
    LOGI("ThermalTool: list_thermal_zones");

    std::vector<ThermalZone> zones = scanThermalZones();

    std::string json = "[";
    for (std::size_t i = 0; i < zones.size(); ++i) {
        if (i > 0) { json += ','; }
        char entry[256];
        std::snprintf(entry, sizeof(entry),
            "{\"zone\":%s,\"type\":%s,\"temp_c\":%.1f}",
            jsonStr(zones[i].name).c_str(),
            jsonStr(zones[i].type).c_str(),
            zones[i].tempC);
        json += entry;
    }
    json += ']';
    *out = json;
    return true;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void registerThermalTools(EaToolRegistry* registry) {
    registry->registerTool(
        "read_cpu_temperature",
        "Read the primary CPU / SoC temperature in Celsius and Fahrenheit "
        "from the kernel's thermal subsystem (/sys/class/thermal/thermal_zone0). "
        "Falls back to the first CPU-related zone if zone0 is unavailable.",
        {},
        handleReadCpuTemperature);

    registry->registerTool(
        "list_thermal_zones",
        "List all thermal zones exposed by the kernel, including their "
        "zone name, type (e.g. cpu-thermal, x86_pkg_temp), and current "
        "temperature in Celsius.",
        {},
        handleListThermalZones);

    LOGI("ThermalTool: registered read_cpu_temperature, list_thermal_zones");
}

}  // namespace example
}  // namespace embedagent

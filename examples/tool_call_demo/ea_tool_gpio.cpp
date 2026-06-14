#include "ea_tool_gpio.h"

#include <ea_tool_args.h>
#include <ea_tool_builder.h>
#include <ea_tool_registry.h>
#include <ev_logger.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace embedagent {
namespace example {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

static std::string gpioDir(int pin) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d", pin);
    return buf;
}

// Returns true if the pin is already exported (sysfs dir exists).
static bool gpioIsExported(int pin) {
    std::ifstream f(gpioDir(pin) + "/value");
    return f.is_open();
}

// Export a GPIO pin via /sys/class/gpio/export.
// Returns true on success or if already exported.
static bool gpioExport(int pin) {
    if (gpioIsExported(pin)) { return true; }

    std::ofstream f("/sys/class/gpio/export");
    if (!f.is_open()) { return false; }
    f << pin;
    return f.good();
}

static bool gpioWriteFile(const std::string& path, const std::string& value) {
    std::ofstream f(path.c_str());
    if (!f.is_open()) { return false; }
    f << value;
    return f.good();
}

static std::string gpioReadFile(const std::string& path) {
    std::ifstream f(path.c_str());
    std::string val;
    if (f.is_open()) { f >> val; }
    return val;
}

}  // namespace

// ---------------------------------------------------------------------------
// Tool handlers
// ---------------------------------------------------------------------------

static bool handleGpioRead(const EaToolArgs& args, std::string* out) {
    int pin = args.getInt("pin", -1);
    LOGI("GpioTool: gpio_read(pin=%d)", pin);

    if (pin < 0) {
        *out = "{\"error\":\"invalid pin number\"}";
        return true;
    }

    if (!gpioIsExported(pin) && !gpioExport(pin)) {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
            "{\"error\":\"cannot export pin %d: %s\"}", pin, std::strerror(errno));
        *out = buf;
        return true;
    }

    std::string valStr = gpioReadFile(gpioDir(pin) + "/value");
    if (valStr.empty()) {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
            "{\"error\":\"cannot read pin %d\"}", pin);
        *out = buf;
        return true;
    }

    std::string dirStr = gpioReadFile(gpioDir(pin) + "/direction");
    char buf[128];
    std::snprintf(buf, sizeof(buf),
        "{\"pin\":%d,\"value\":%s,\"direction\":\"%s\"}",
        pin, valStr.c_str(),
        dirStr.empty() ? "unknown" : dirStr.c_str());
    *out = buf;
    return true;
}

static bool handleGpioWrite(const EaToolArgs& args, std::string* out) {
    int pin = args.getInt("pin", -1);
    int val = args.getInt("value", -1);
    LOGI("GpioTool: gpio_write(pin=%d, value=%d)", pin, val);

    if (pin < 0 || (val != 0 && val != 1)) {
        *out = "{\"error\":\"invalid pin or value (must be 0 or 1)\"}";
        return true;
    }

    if (!gpioIsExported(pin) && !gpioExport(pin)) {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
            "{\"error\":\"cannot export pin %d: %s\"}", pin, std::strerror(errno));
        *out = buf;
        return true;
    }

    // Ensure direction is output before writing.
    std::string dirPath = gpioDir(pin) + "/direction";
    std::string curDir  = gpioReadFile(dirPath);
    if (curDir != "out") {
        gpioWriteFile(dirPath, "out");
    }

    std::string valStr = (val == 1) ? "1" : "0";
    if (!gpioWriteFile(gpioDir(pin) + "/value", valStr)) {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
            "{\"error\":\"write to pin %d failed: %s\"}", pin, std::strerror(errno));
        *out = buf;
        return true;
    }

    char buf[64];
    std::snprintf(buf, sizeof(buf),
        "{\"ok\":true,\"pin\":%d,\"value\":%d}", pin, val);
    *out = buf;
    return true;
}

static bool handleGpioSetDirection(const EaToolArgs& args, std::string* out) {
    int         pin = args.getInt("pin", -1);
    std::string dir = args.getString("direction", "");
    LOGI("GpioTool: gpio_set_direction(pin=%d, direction=%s)", pin, dir.c_str());

    if (pin < 0) {
        *out = "{\"error\":\"invalid pin number\"}";
        return true;
    }
    if (dir != "in" && dir != "out") {
        *out = "{\"error\":\"direction must be \\\"in\\\" or \\\"out\\\"\"}";
        return true;
    }

    if (!gpioIsExported(pin) && !gpioExport(pin)) {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
            "{\"error\":\"cannot export pin %d: %s\"}", pin, std::strerror(errno));
        *out = buf;
        return true;
    }

    if (!gpioWriteFile(gpioDir(pin) + "/direction", dir)) {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
            "{\"error\":\"set direction on pin %d failed: %s\"}",
            pin, std::strerror(errno));
        *out = buf;
        return true;
    }

    char buf[64];
    std::snprintf(buf, sizeof(buf),
        "{\"ok\":true,\"pin\":%d,\"direction\":\"%s\"}", pin, dir.c_str());
    *out = buf;
    return true;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void registerGpioTools(EaToolRegistry* registry) {
    std::vector<EaToolParam> pinParam;
    {
        EaToolParam p;
        p.name        = "pin";
        p.type        = EaParamType::kInteger;
        p.description = "GPIO pin number (BCM / sysfs index)";
        p.required    = true;
        pinParam.push_back(p);
    }

    registry->registerTool(
        "gpio_read",
        "Read the current logic level (0 or 1) and direction of a GPIO pin "
        "via the Linux sysfs interface. "
        "The pin is auto-exported if not already accessible.",
        pinParam,
        handleGpioRead);

    // gpio_write: pin + value enum
    std::vector<std::string> levels;
    levels.push_back("0");
    levels.push_back("1");

    EaToolSpec writeSpec = EaToolBuilder()
        .name("gpio_write")
        .description(
            "Drive a GPIO output pin to logic high (1) or low (0) via the Linux "
            "sysfs interface. Automatically sets direction to output. "
            "The pin is auto-exported if needed.")
        .param("pin", EaParamType::kInteger,
               "GPIO pin number (BCM / sysfs index)", true)
        .enumParam("value", EaParamType::kInteger,
                   "Output level: 0 = low, 1 = high", levels, true)
        .handler(handleGpioWrite)
        .build();
    registry->registerTool(writeSpec);

    // gpio_set_direction: pin + direction enum
    std::vector<std::string> dirs;
    dirs.push_back("in");
    dirs.push_back("out");

    EaToolSpec dirSpec = EaToolBuilder()
        .name("gpio_set_direction")
        .description(
            "Configure a GPIO pin as an input or output via the Linux sysfs "
            "interface. The pin is auto-exported if needed.")
        .param("pin", EaParamType::kInteger,
               "GPIO pin number (BCM / sysfs index)", true)
        .enumParam("direction", EaParamType::kString,
                   "Pin direction: \"in\" or \"out\"", dirs, true)
        .handler(handleGpioSetDirection)
        .build();
    registry->registerTool(dirSpec);

    LOGI("GpioTool: registered gpio_read, gpio_write, gpio_set_direction");
}

}  // namespace example
}  // namespace embedagent

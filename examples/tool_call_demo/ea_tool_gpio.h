// ea_tool_gpio.h — GPIO control tools for EmbedAgent tool_call_demo
//
// Registers three tools:
//   gpio_read           — read the current logic level of a pin
//   gpio_write          — drive a pin high or low
//   gpio_set_direction  — configure a pin as input or output
//
// Accesses the Linux sysfs GPIO interface (/sys/class/gpio/).
// Pins must be exported before use (the handlers auto-export as needed).
#pragma once

namespace embedagent {
class EaToolRegistry;
}

namespace embedagent {
namespace example {

void registerGpioTools(EaToolRegistry* registry);

}  // namespace example
}  // namespace embedagent

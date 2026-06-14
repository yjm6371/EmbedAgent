// ea_tool_thermal.h — CPU / SoC temperature tools for EmbedAgent tool_call_demo
//
// Registers two tools:
//   read_cpu_temperature  — primary thermal zone temperature (Celsius)
//   list_thermal_zones    — all available thermal zones with temperatures
#pragma once

namespace embedagent {
class EaToolRegistry;
}

namespace embedagent {
namespace example {

void registerThermalTools(EaToolRegistry* registry);

}  // namespace example
}  // namespace embedagent

// ea_tool_sysinfo.h — system information tools for EmbedAgent tool_call_demo
//
// Registers three tools:
//   get_system_info  — uptime and load average  (/proc/uptime, /proc/loadavg)
//   get_memory_info  — RAM usage breakdown       (/proc/meminfo)
//   get_disk_usage   — filesystem usage for path (statvfs)
#pragma once

namespace embedagent {
class EaToolRegistry;
}

namespace embedagent {
namespace example {

void registerSysinfoTools(EaToolRegistry* registry);

}  // namespace example
}  // namespace embedagent

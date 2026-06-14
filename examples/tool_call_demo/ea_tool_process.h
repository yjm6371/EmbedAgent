// ea_tool_process.h — process management tools for EmbedAgent tool_call_demo
//
// Registers two tools:
//   list_top_processes  — top N processes sorted by RSS memory usage
//   get_process_info    — detailed info for a specific PID
#pragma once

namespace embedagent {
class EaToolRegistry;
}

namespace embedagent {
namespace example {

void registerProcessTools(EaToolRegistry* registry);

}  // namespace example
}  // namespace embedagent

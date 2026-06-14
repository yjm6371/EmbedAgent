// ea_tool_network.h — network information tools for EmbedAgent tool_call_demo
//
// Registers two tools:
//   get_network_interfaces  — list all network interfaces with IP, MAC, state
//   get_interface_stats     — TX/RX bytes, packets, errors for one interface
#pragma once

namespace embedagent {
class EaToolRegistry;
}

namespace embedagent {
namespace example {

void registerNetworkTools(EaToolRegistry* registry);

}  // namespace example
}  // namespace embedagent

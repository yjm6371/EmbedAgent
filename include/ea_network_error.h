// ea_network_error.h — classify LLM HTTP failures for offline queueing
#pragma once

#include <string>

namespace embedagent {

// True when the failure should be queued for later (transport / 5xx / 429).
bool isQueueableLlmError(const std::string& error);

}  // namespace embedagent

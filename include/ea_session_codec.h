// ea_session_codec.h — serialize / deserialize EaSession to JSON
#pragma once

#include <ea_session.h>

#include <ev_llm_json.h>

#include <string>
#include <vector>

namespace embedagent {

struct EaSessionSnapshot {
    int         version {1};
    std::string systemPrompt;
    EaSessionOptions opts;
    std::vector<cppev::ext::EvLlmMessage> messages;
};

bool serializeSession(const EaSession& session, std::string* jsonOut);
bool loadSessionSnapshot(const std::string& json, EaSessionSnapshot* out);
bool applySessionSnapshot(const EaSessionSnapshot& snap, EaSession* session);

}  // namespace embedagent

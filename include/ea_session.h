// ea_session.h — multi-turn conversation history with trimming
#pragma once

#include <ev_llm_json.h>

#include <cstddef>
#include <string>
#include <vector>

namespace embedagent {

struct EaSessionOptions {
    std::size_t maxMessages     {40};
    std::size_t maxApproxTokens {8000};  // rough char/4 budget
};

class EaSession {
public:
    explicit EaSession(const EaSessionOptions& opts = EaSessionOptions());

    void setOptions(const EaSessionOptions& opts);
    const EaSessionOptions& options() const { return opts_; }

    void setSystemPrompt(const std::string& prompt);
    const std::string& systemPrompt() const { return systemPrompt_; }

    void appendUser(const std::string& content);
    bool rollbackLastUser();
    void appendAssistant(const std::string& content);
    void appendAssistant(const std::string& content,
                         const std::vector<cppev::ext::EvLlmToolCall>& toolCalls);
    void appendToolResult(const std::string& toolCallId,
                          const std::string& content);

    void clear();
    void trim();
    void restoreMessages(const std::vector<cppev::ext::EvLlmMessage>& msgs);

    const std::vector<cppev::ext::EvLlmMessage>& messages() const {
        return messages_;
    }

    std::vector<cppev::ext::EvLlmMessage> snapshot() const;

private:
    void rebuildSystemMessage();
    std::size_t approxTokenCount() const;

    EaSessionOptions                    opts_;
    std::string                         systemPrompt_;
    std::vector<cppev::ext::EvLlmMessage> messages_;
    bool                                hasSystemMessage_ {false};
};

}  // namespace embedagent

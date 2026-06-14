#include <ea_session.h>

namespace embedagent {

EaSession::EaSession(const EaSessionOptions& opts)
    : opts_(opts)
{}

void EaSession::setOptions(const EaSessionOptions& opts) {
    opts_ = opts;
    trim();
}

void EaSession::setSystemPrompt(const std::string& prompt) {
    systemPrompt_ = prompt;
    rebuildSystemMessage();
}

void EaSession::rebuildSystemMessage() {
    if (systemPrompt_.empty()) {
        if (hasSystemMessage_ && !messages_.empty() &&
            messages_.front().role == "system") {
            messages_.erase(messages_.begin());
            hasSystemMessage_ = false;
        }
        return;
    }

    cppev::ext::EvLlmMessage systemMsg;
    systemMsg.role = "system";
    systemMsg.content = systemPrompt_;

    if (hasSystemMessage_ && !messages_.empty() &&
        messages_.front().role == "system") {
        messages_.front() = systemMsg;
    } else {
        messages_.insert(messages_.begin(), systemMsg);
        hasSystemMessage_ = true;
    }
}

void EaSession::appendUser(const std::string& content) {
    cppev::ext::EvLlmMessage msg;
    msg.role = "user";
    msg.content = content;
    messages_.push_back(msg);
    trim();
}

bool EaSession::rollbackLastUser() {
    if (messages_.empty()) {
        return false;
    }

    if (messages_.back().role != "user") {
        return false;
    }

    messages_.pop_back();
    return true;
}

void EaSession::appendAssistant(const std::string& content) {
    cppev::ext::EvLlmMessage msg;
    msg.role = "assistant";
    msg.content = content;
    messages_.push_back(msg);
    trim();
}

void EaSession::appendAssistant(
    const std::string& content,
    const std::vector<cppev::ext::EvLlmToolCall>& toolCalls) {
    cppev::ext::EvLlmMessage msg;
    msg.role = "assistant";
    msg.content = content;
    msg.toolCalls = toolCalls;
    messages_.push_back(msg);
    trim();
}

void EaSession::appendToolResult(const std::string& toolCallId,
                                 const std::string& content) {
    cppev::ext::EvLlmMessage msg;
    msg.role = "tool";
    msg.toolCallId = toolCallId;
    msg.content = content;
    messages_.push_back(msg);
    trim();
}

void EaSession::clear() {
    messages_.clear();
    hasSystemMessage_ = false;
    rebuildSystemMessage();
}

void EaSession::restoreMessages(
    const std::vector<cppev::ext::EvLlmMessage>& msgs) {
    const std::size_t startIdx = hasSystemMessage_ ? 1 : 0;
    if (messages_.size() > startIdx) {
        messages_.erase(messages_.begin() +
                        static_cast<std::ptrdiff_t>(startIdx),
                        messages_.end());
    }

    for (std::size_t i = 0; i < msgs.size(); ++i) {
        messages_.push_back(msgs[i]);
    }
    trim();
}

std::size_t EaSession::approxTokenCount() const {
    std::size_t chars = 0;
    for (std::size_t i = 0; i < messages_.size(); ++i) {
        chars += messages_[i].content.size();
        for (std::size_t j = 0; j < messages_[i].toolCalls.size(); ++j) {
            chars += messages_[i].toolCalls[j].arguments.size();
        }
    }
    return chars / 4 + 1;
}

void EaSession::trim() {
    const std::size_t startIdx = hasSystemMessage_ ? 1 : 0;

    while (messages_.size() - startIdx > opts_.maxMessages) {
        messages_.erase(messages_.begin() + static_cast<std::ptrdiff_t>(startIdx));
    }

    while (messages_.size() > startIdx &&
           approxTokenCount() > opts_.maxApproxTokens) {
        messages_.erase(messages_.begin() + static_cast<std::ptrdiff_t>(startIdx));
    }
}

std::vector<cppev::ext::EvLlmMessage> EaSession::snapshot() const {
    return messages_;
}

}  // namespace embedagent

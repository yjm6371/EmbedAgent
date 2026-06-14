#include <ea_demo_io.h>

#include <ev_logger.h>

namespace embedagent {

void EaStreamPrinter::beginSegment() {
    if (segmentStarted_) {
        std::fputc('\n', stdout);
        std::fflush(stdout);
    }
    segmentStarted_ = false;
}

void EaStreamPrinter::feed(const std::string& delta) {
    if (delta.empty()) {
        return;
    }
    if (!segmentStarted_) {
        std::fputs("\nAssistant: ", stdout);
        segmentStarted_ = true;
    }
    wroteContent_ = true;
    std::fwrite(delta.data(), 1, delta.size(), stdout);
    std::fflush(stdout);
}

void EaStreamPrinter::finish() {
    if (wroteContent_) {
        std::fputc('\n', stdout);
        std::fflush(stdout);
    }
}

void printUserPrompt(const std::string& prompt) {
    std::fprintf(stdout, "\nUser: %s\n", prompt.c_str());
    std::fflush(stdout);
}

void printSessionTranscript(const EaSession& session, const char* tag) {
    const std::vector<cppev::ext::EvLlmMessage>& msgs = session.messages();
    LOGI("%s: --- session transcript (%zu messages) ---", tag, msgs.size());
    for (std::size_t i = 0; i < msgs.size(); ++i) {
        const cppev::ext::EvLlmMessage& m = msgs[i];
        if (m.role == "tool") {
            LOGI("%s:   [%zu] tool id=%s result=%s",
                 tag, i, m.toolCallId.c_str(), m.content.c_str());
        } else if (m.role == "assistant" && !m.toolCalls.empty()) {
            LOGI("%s:   [%zu] assistant tool_calls=%zu",
                 tag, i, m.toolCalls.size());
            for (std::size_t j = 0; j < m.toolCalls.size(); ++j) {
                const cppev::ext::EvLlmToolCall& tc = m.toolCalls[j];
                LOGI("%s:       -> %s(%s)",
                     tag, tc.name.c_str(), tc.arguments.c_str());
            }
        } else {
            LOGI("%s:   [%zu] %s: %s",
                 tag, i, m.role.c_str(), m.content.c_str());
        }
    }
}

}  // namespace embedagent

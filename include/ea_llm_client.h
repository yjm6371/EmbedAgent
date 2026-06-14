// ea_llm_client.h — HTTP streaming wrapper for chat completions
#pragma once

#include <ev_http_client.h>
#include <ev_llm_json.h>
#include <ev_sse_parser.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace embedagent {

// If url is an API base (no "completions" path), append the chat-completions path.
// OpenAI bases get /v1/chat/completions; most others get /chat/completions.
std::string resolveChatCompletionsUrl(const std::string& url);

struct EaLlmClientOptions {
    std::string              url;
    std::string              apiKey;
    std::string              model;
    bool                     stream {true};
    long                     totalTimeoutSec {120};
    cppev::ext::EvHttpRetryPolicy retry;
    std::vector<std::string> mockResponses;  // non-empty: skip HTTP, one per chat()
};

struct EaLlmCompletion {
    std::string                        content;
    std::string                        finishReason;
    std::vector<cppev::ext::EvLlmToolCall> toolCalls;
};

using EaStreamChunkCallback =
    std::function<void(const cppev::ext::EvLlmStreamDelta& delta)>;
using EaCompletionCallback =
    std::function<void(bool ok, const EaLlmCompletion& result,
                       const std::string& error)>;

// call in loop thread only
class EaLlmClient {
public:
    EaLlmClient(cppev::EvEventLoop* loop, const EaLlmClientOptions& opts);
    ~EaLlmClient();

    EaLlmClient(const EaLlmClient&)            = delete;
    EaLlmClient& operator=(const EaLlmClient&) = delete;

    void chat(const std::vector<cppev::ext::EvLlmMessage>& messages,
              const std::vector<cppev::ext::EvLlmToolDef>& tools,
              EaStreamChunkCallback onChunk,
              EaCompletionCallback onDone);

    void cancel();

private:
    struct ChatContext;

    cppev::EvEventLoop*                    loop_;
    EaLlmClientOptions                     opts_;
    std::unique_ptr<cppev::ext::EvHttpClient> client_;
    std::shared_ptr<ChatContext>           activeCtx_;
    std::size_t                            mockIndex_ {0};
};

}  // namespace embedagent

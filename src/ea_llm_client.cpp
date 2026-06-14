#include <ea_llm_client.h>

#include <ev_logger.h>

#include <cstring>
#include <string>

namespace embedagent {

namespace {

std::string stripTrailingSlash(std::string url) {
    while (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    return url;
}

bool endsWith(const std::string& s, const char* suffix) {
    const std::size_t len = std::strlen(suffix);
    return s.size() >= len && s.compare(s.size() - len, len, suffix) == 0;
}

}  // namespace

std::string resolveChatCompletionsUrl(const std::string& url) {
    if (url.empty()) {
        return url;
    }

    // If the URL already points at a completions endpoint, use it verbatim.
    if (url.find("completions") != std::string::npos) {
        return url;
    }

    const std::string base = stripTrailingSlash(url);

    // Caller already included the versioned prefix (e.g. "…/v1").
    if (endsWith(base, "/v1")) {
        return base + "/chat/completions";
    }

    // Default: virtually all OpenAI-compatible APIs (OpenAI, DeepSeek, Groq,
    // Together, Mistral, …) mount the chat endpoint at /v1/chat/completions.
    return base + "/v1/chat/completions";
}

struct EaLlmClient::ChatContext {
    cppev::ext::EvSseParser              parser;
    cppev::ext::EvLlmStreamAccumulator   accumulator;
    EaStreamChunkCallback                onChunk;
    EaCompletionCallback                 onDone;
    bool                                 streamMode {true};
};

EaLlmClient::EaLlmClient(cppev::EvEventLoop* loop, const EaLlmClientOptions& opts)
    : loop_(loop)
    , opts_(opts)
    , client_(new cppev::ext::EvHttpClient(loop_))
{}

EaLlmClient::~EaLlmClient() {
    cancel();
}

void EaLlmClient::cancel() {
    activeCtx_.reset();
}

void EaLlmClient::chat(const std::vector<cppev::ext::EvLlmMessage>& messages,
                       const std::vector<cppev::ext::EvLlmToolDef>& tools,
                       EaStreamChunkCallback onChunk,
                       EaCompletionCallback onDone) {
    if (!onDone) {
        return;
    }

    if (!opts_.mockResponses.empty()) {
        if (mockIndex_ >= opts_.mockResponses.size()) {
            onDone(false, EaLlmCompletion(),
                   "EaLlmClient: mock responses exhausted");
            return;
        }

        const std::string& mockBody = opts_.mockResponses[mockIndex_];
        mockIndex_ += 1;

        cppev::ext::EvLlmChatResponse parsed;
        if (!cppev::ext::parseChatResponse(mockBody, &parsed)) {
            onDone(false, EaLlmCompletion(),
                   "EaLlmClient: mock parseChatResponse failed");
            return;
        }

        if (onChunk && !parsed.content.empty()) {
            cppev::ext::EvLlmStreamDelta delta;
            delta.content = parsed.content;
            onChunk(delta);
        }

        EaLlmCompletion result;
        result.content = parsed.content;
        result.finishReason = parsed.finishReason;
        result.toolCalls = parsed.toolCalls;
        onDone(true, result, std::string());
        (void)messages;
        (void)tools;
        return;
    }

    if (opts_.url.empty() || opts_.apiKey.empty() || opts_.model.empty()) {
        onDone(false, EaLlmCompletion(), "EaLlmClient: url/apiKey/model required");
        return;
    }

    cppev::ext::EvLlmChatRequest req;
    req.model = opts_.model;
    req.messages = messages;
    req.tools = tools;
    req.stream = opts_.stream;
    if (!tools.empty()) {
        req.toolChoice = "auto";
    }

    std::string body;
    if (!cppev::ext::buildChatRequestJson(req, &body)) {
        onDone(false, EaLlmCompletion(), "EaLlmClient: buildChatRequestJson failed");
        return;
    }

    cppev::ext::EvHttpRequest httpReq;
    httpReq.method = "POST";
    httpReq.url = resolveChatCompletionsUrl(opts_.url);
    if (httpReq.url != opts_.url) {
        LOGI("EaLlmClient: resolved URL %s", httpReq.url.c_str());
    }
    httpReq.body = body;
    httpReq.headers["Authorization"] = "Bearer " + opts_.apiKey;
    httpReq.headers["Content-Type"] = "application/json";
    httpReq.totalTimeoutSec = opts_.totalTimeoutSec;
    httpReq.retry = opts_.retry;

    if (!opts_.stream) {
        client_->request(
            httpReq,
            [onDone](const std::string& /*url*/, int code,
                     const std::string& respBody, const std::string& err) {
                if (!err.empty()) {
                    onDone(false, EaLlmCompletion(), err);
                    return;
                }
                if (code < 200 || code >= 300) {
                    // Log the body so callers can see the API's error detail.
                    if (!respBody.empty()) {
                        LOGE("EaLlmClient: HTTP %d — %s", code, respBody.c_str());
                    }
                    onDone(false, EaLlmCompletion(),
                           "EaLlmClient: HTTP " + std::to_string(code));
                    return;
                }

                cppev::ext::EvLlmChatResponse parsed;
                if (!cppev::ext::parseChatResponse(respBody, &parsed)) {
                    onDone(false, EaLlmCompletion(),
                           "EaLlmClient: parseChatResponse failed");
                    return;
                }

                EaLlmCompletion result;
                result.content = parsed.content;
                result.finishReason = parsed.finishReason;
                result.toolCalls = parsed.toolCalls;
                onDone(true, result, std::string());
            });
        return;
    }

    std::shared_ptr<ChatContext> ctx(new ChatContext());
    ctx->onChunk = onChunk;
    ctx->onDone = onDone;
    ctx->streamMode = true;
    activeCtx_ = ctx;

    ctx->parser.setDataCallback(
        [ctx](const std::string& payload) {
            cppev::ext::EvLlmStreamDelta delta;
            if (!cppev::ext::parseStreamDelta(payload, &delta)) {
                return;
            }

            ctx->accumulator.applyDelta(delta);

            if (ctx->onChunk) {
                ctx->onChunk(delta);
            }
        });

    client_->requestStream(
        httpReq,
        [ctx](const char* data, std::size_t len) {
            return ctx->parser.feed(data, len);
        },
        [ctx](const std::string& /*url*/, int code,
              const std::string& body, const std::string& err) {
            EaCompletionCallback doneCb = ctx->onDone;

            if (!err.empty()) {
                if (doneCb) {
                    doneCb(false, EaLlmCompletion(), err);
                }
                return;
            }
            if (code < 200 || code >= 300) {
                // Log the body so callers can see the API's error detail.
                if (!body.empty()) {
                    LOGE("EaLlmClient: HTTP %d — %s", code, body.c_str());
                }
                if (doneCb) {
                    doneCb(false, EaLlmCompletion(),
                           "EaLlmClient: HTTP " + std::to_string(code));
                }
                return;
            }

            EaLlmCompletion result;
            result.content = ctx->accumulator.content();
            result.finishReason = ctx->accumulator.finishReason();
            result.toolCalls = ctx->accumulator.toolCalls();

            if (doneCb) {
                doneCb(true, result, std::string());
            }
        });
}

}  // namespace embedagent

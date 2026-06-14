// ea_agent.h — multi-turn agent orchestrator with tool calling loop
#pragma once

#include <ea_llm_client.h>
#include <ea_prompt_template.h>
#include <ea_session.h>
#include <ea_tool_registry.h>

#include <functional>
#include <memory>
#include <string>

namespace embedagent {

class EaStorage;

struct EaAgentOptions {
    EaLlmClientOptions llm;
    EaSessionOptions   session;
    EaPromptTemplate   systemTemplate;
    std::string        deviceName {"embed-device"};
    int                maxToolRoundTrips {8};
    EaStorage*         storage {nullptr};
    bool               persistSession {false};
};

using EaAgentTextCallback =
    std::function<void(const std::string& delta)>;
using EaAgentDoneCallback =
    std::function<void(bool ok, const std::string& error)>;
using EaAgentRoundCallback =
    std::function<void(int roundTrip)>;

// call in loop thread only
class EaAgent {
public:
    EaAgent(cppev::EvEventLoop* loop, const EaAgentOptions& opts);
    ~EaAgent();

    EaAgent(const EaAgent&)            = delete;
    EaAgent& operator=(const EaAgent&) = delete;

    void setToolRegistry(EaToolRegistry* registry);
    void setRoundStartCallback(EaAgentRoundCallback cb);

    // Convenience: override the rendered system prompt directly.
    // Calling this bypasses systemTemplate / deviceName rendering.
    // Must be called before submitUserMessage.
    void setSystemPrompt(const std::string& prompt);

    void submitUserMessage(const std::string& text,
                           EaAgentTextCallback onText,
                           EaAgentDoneCallback onDone,
                           bool skipAppendUser = false);

    void cancel();
    EaSession& session();

private:
    struct TurnContext {
        EaAgentTextCallback onText;
        EaAgentDoneCallback onDone;
        int                 roundTrip {0};
    };

    void refreshSystemPrompt();
    void runTurn(const std::shared_ptr<TurnContext>& ctx);
    void finishTurn(const std::shared_ptr<TurnContext>& ctx,
                    bool ok, const std::string& error);

    cppev::EvEventLoop*              loop_;
    EaAgentOptions                   opts_;
    EaSession                        session_;
    std::unique_ptr<EaLlmClient>   llmClient_;
    EaToolRegistry*                  registry_ {nullptr};
    EaAgentRoundCallback             onRoundStart_;
    std::shared_ptr<TurnContext>     activeTurn_;
};

}  // namespace embedagent

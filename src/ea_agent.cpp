#include <ea_agent.h>

#include <ea_storage.h>

#include <ev_logger.h>
#include <ev_timestamp.h>

namespace embedagent {

EaAgent::EaAgent(cppev::EvEventLoop* loop, const EaAgentOptions& opts)
    : loop_(loop)
    , opts_(opts)
    , session_(opts.session)
    , llmClient_(new EaLlmClient(loop_, opts.llm))
{
    bool loaded = false;
    if (opts_.storage && opts_.persistSession) {
        loaded = opts_.storage->loadSession(&session_);
        if (loaded) {
            LOGI("EaAgent: loaded persisted session");
        }
    }

    if (!loaded) {
        refreshSystemPrompt();
    }
}

EaAgent::~EaAgent() {
    cancel();
}

void EaAgent::setToolRegistry(EaToolRegistry* registry) {
    registry_ = registry;
}

void EaAgent::setRoundStartCallback(EaAgentRoundCallback cb) {
    onRoundStart_ = cb;
}

void EaAgent::setSystemPrompt(const std::string& prompt) {
    session_.setSystemPrompt(prompt);
}

EaSession& EaAgent::session() {
    return session_;
}

void EaAgent::refreshSystemPrompt() {
    EaPromptTemplate tmpl = opts_.systemTemplate;
    tmpl.setVariable("device_name", opts_.deviceName);
    tmpl.setVariable("current_time", cppev::EvTimestamp::now().toString());
    session_.setSystemPrompt(tmpl.render());
}

void EaAgent::submitUserMessage(const std::string& text,
                                EaAgentTextCallback onText,
                                EaAgentDoneCallback onDone,
                                bool skipAppendUser) {
    if (activeTurn_) {
        if (onDone) {
            onDone(false, "EaAgent: request already in progress");
        }
        return;
    }

    refreshSystemPrompt();
    if (!skipAppendUser) {
        session_.appendUser(text);
    }

    std::shared_ptr<TurnContext> ctx(new TurnContext());
    ctx->onText = onText;
    ctx->onDone = onDone;
    ctx->roundTrip = 0;
    activeTurn_ = ctx;

    runTurn(ctx);
}

void EaAgent::cancel() {
    if (llmClient_) {
        llmClient_->cancel();
    }
    activeTurn_.reset();
}

void EaAgent::runTurn(const std::shared_ptr<TurnContext>& ctx) {
    if (!ctx) {
        return;
    }

    // Guard against runaway tool-calling loops.
    if (ctx->roundTrip >= opts_.maxToolRoundTrips) {
        finishTurn(ctx, false, "EaAgent: max tool round trips exceeded");
        return;
    }

    if (onRoundStart_) {
        onRoundStart_(ctx->roundTrip);
    }

    std::vector<cppev::ext::EvLlmToolDef> tools;
    if (registry_ && !registry_->empty()) {
        tools = registry_->toolDefinitions();
    }

    // Send the current session snapshot (including previous tool results) to
    // the LLM and wait for its response.
    llmClient_->chat(
        session_.snapshot(),
        tools,
        [ctx](const cppev::ext::EvLlmStreamDelta& delta) {
            if (ctx->onText && !delta.content.empty()) {
                ctx->onText(delta.content);
            }
        },
        [this, ctx](bool ok, const EaLlmCompletion& result,
                    const std::string& error) {
            if (!ok) {
                finishTurn(ctx, false, error);
                return;
            }

            const bool hasToolCalls = !result.toolCalls.empty();
            const bool wantsTools =
                result.finishReason == "tool_calls" || hasToolCalls;

            if (wantsTools && hasToolCalls) {
                // Tool-calling round: execute each requested tool locally,
                // append the results to the session, then recurse for the
                // next LLM turn.
                if (!registry_) {
                    finishTurn(ctx, false, "EaAgent: tool_calls but no registry");
                    return;
                }

                session_.appendAssistant(result.content, result.toolCalls);

                for (std::size_t i = 0; i < result.toolCalls.size(); ++i) {
                    const cppev::ext::EvLlmToolCall& tc = result.toolCalls[i];
                    std::string toolResult;
                    if (!registry_->invoke(tc.name, tc.arguments, &toolResult)) {
                        finishTurn(ctx, false,
                                   "EaAgent: tool invoke failed: " + tc.name);
                        return;
                    }

                    session_.appendToolResult(tc.id, toolResult);
                }

                ctx->roundTrip += 1;
                runTurn(ctx);
                return;
            }

            // Final turn: the model returned a natural-language reply.
            session_.appendAssistant(result.content);
            finishTurn(ctx, true, std::string());
        });
}

void EaAgent::finishTurn(const std::shared_ptr<TurnContext>& ctx,
                         bool ok, const std::string& error) {
    EaAgentDoneCallback doneCb;
    if (ctx) {
        doneCb = ctx->onDone;
    }

    activeTurn_.reset();

    if (ok && opts_.storage && opts_.persistSession) {
        if (!opts_.storage->saveSession(session_)) {
            LOGW("EaAgent: failed to persist session");
        }
    }

    if (doneCb) {
        doneCb(ok, error);
    }
}

}  // namespace embedagent

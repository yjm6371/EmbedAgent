#include <gtest/gtest.h>

#include <ea_agent.h>
#include <ea_prompt_template.h>
#include <ea_tool_registry.h>

#include <ev_event_loop.h>

using embedagent::EaAgent;
using embedagent::EaAgentOptions;
using embedagent::EaPromptTemplate;
using embedagent::EaToolRegistry;
using embedagent::EaToolSpec;

namespace {

void registerStatusTool(EaToolRegistry* registry) {
    EaToolSpec spec;
    spec.def.name = "get_device_status";
    spec.def.description = "Return device uptime";
    spec.def.parametersJson = "{\"type\":\"object\",\"properties\":{}}";
    spec.handler = [](const std::string& /*args*/, std::string* out) {
        *out = "{\"uptime_sec\":3600,\"temp_c\":42}";
        return true;
    };
    ASSERT_TRUE(registry->registerTool(spec));
}

}  // namespace

TEST(EaAgentMockTest, ToolCallingLoop) {
    cppev::EvEventLoop loop;

    EaAgentOptions opts;
    opts.llm.model = "mock-model";
    opts.llm.stream = false;
    opts.llm.mockResponses.push_back(
        "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":null,"
        "\"tool_calls\":[{\"id\":\"call_1\",\"type\":\"function\","
        "\"function\":{\"name\":\"get_device_status\",\"arguments\":\"{}\"}}]},"
        "\"finish_reason\":\"tool_calls\"}]}");
    opts.llm.mockResponses.push_back(
        "{\"choices\":[{\"message\":{\"role\":\"assistant\","
        "\"content\":\"Device uptime is 3600 seconds.\"},"
        "\"finish_reason\":\"stop\"}]}");

    EaPromptTemplate tmpl;
    tmpl.setTemplate("You are {{device_name}}.");
    opts.systemTemplate = tmpl;
    opts.deviceName = "test-device";

    EaToolRegistry registry;
    registerStatusTool(&registry);

    EaAgent agent(&loop, opts);
    agent.setToolRegistry(&registry);

    std::string streamed;
    bool done = false;
    bool ok = false;
    std::string error;

    agent.submitUserMessage(
        "What is the device status?",
        [&streamed](const std::string& delta) { streamed += delta; },
        [&done, &ok, &error](bool success, const std::string& err) {
            done = true;
            ok = success;
            error = err;
        });

    ASSERT_TRUE(done);
    EXPECT_TRUE(ok) << error;
    EXPECT_EQ(streamed, "Device uptime is 3600 seconds.");
    ASSERT_GE(agent.session().messages().size(), 4u);
    EXPECT_EQ(agent.session().messages().back().role, "assistant");
}

#include <gtest/gtest.h>

#include <ea_tool_registry.h>

using embedagent::EaToolRegistry;
using embedagent::EaToolSpec;

TEST(EaToolRegistryTest, RegisterAndInvoke) {
    EaToolRegistry registry;

    EaToolSpec spec;
    spec.def.name = "get_status";
    spec.def.description = "Return status";
    spec.def.parametersJson = "{\"type\":\"object\"}";
    spec.handler = [](const std::string& /*args*/, std::string* out) {
        *out = "{\"uptime\":3600}";
        return true;
    };

    ASSERT_TRUE(registry.registerTool(spec));
    EXPECT_FALSE(registry.registerTool(spec));

    std::string result;
    ASSERT_TRUE(registry.invoke("get_status", "{}", &result));
    EXPECT_EQ(result, "{\"uptime\":3600}");
}

TEST(EaToolRegistryTest, ToolDefinitions) {
    EaToolRegistry registry;

    EaToolSpec spec;
    spec.def.name = "ping";
    spec.def.description = "Ping";
    spec.handler = [](const std::string&, std::string* out) {
        *out = "pong";
        return true;
    };
    registry.registerTool(spec);

    std::vector<cppev::ext::EvLlmToolDef> defs = registry.toolDefinitions();
    ASSERT_EQ(defs.size(), 1u);
    EXPECT_EQ(defs[0].name, "ping");
}

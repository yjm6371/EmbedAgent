#include <gtest/gtest.h>

#include <ea_session.h>

using embedagent::EaSession;
using embedagent::EaSessionOptions;

TEST(EaSessionTest, SystemPromptAndUserMessage) {
    EaSession session;
    session.setSystemPrompt("You are a device assistant.");
    session.appendUser("hello");

    ASSERT_EQ(session.messages().size(), 2u);
    EXPECT_EQ(session.messages()[0].role, "system");
    EXPECT_EQ(session.messages()[1].role, "user");
    EXPECT_EQ(session.messages()[1].content, "hello");
}

TEST(EaSessionTest, TrimByMaxMessages) {
    EaSessionOptions opts;
    opts.maxMessages = 2;
    EaSession session(opts);
    session.setSystemPrompt("system");

    session.appendUser("u1");
    session.appendUser("u2");
    session.appendUser("u3");

    ASSERT_EQ(session.messages().size(), 3u);
    EXPECT_EQ(session.messages()[0].role, "system");
    EXPECT_EQ(session.messages()[1].content, "u2");
    EXPECT_EQ(session.messages()[2].content, "u3");
}

TEST(EaSessionTest, ToolResultRoundTrip) {
    EaSession session;

    cppev::ext::EvLlmToolCall tc;
    tc.id = "call_1";
    tc.name = "get_status";
    tc.arguments = "{}";
    session.appendAssistant("", std::vector<cppev::ext::EvLlmToolCall>(1, tc));
    session.appendToolResult("call_1", "{\"ok\":true}");

    ASSERT_EQ(session.messages().size(), 2u);
    EXPECT_EQ(session.messages()[1].role, "tool");
    EXPECT_EQ(session.messages()[1].toolCallId, "call_1");
}

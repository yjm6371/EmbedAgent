#include <gtest/gtest.h>

#include <ea_session.h>
#include <ea_session_codec.h>

#include <ev_llm_json.h>

using cppev::ext::EvLlmMessage;
using cppev::ext::EvLlmToolCall;
using embedagent::EaSession;
using embedagent::EaSessionSnapshot;
using embedagent::applySessionSnapshot;
using embedagent::loadSessionSnapshot;
using embedagent::serializeSession;

TEST(EaSessionCodecTest, RoundTripWithToolMessages) {
    EaSession session;
    session.setSystemPrompt("You are a test agent.");

    session.appendUser("status?");
    EvLlmToolCall tc;
    tc.id = "call_1";
    tc.name = "get_status";
    tc.arguments = "{}";
    session.appendAssistant(std::string(), std::vector<EvLlmToolCall>(1, tc));
    session.appendToolResult("call_1", "{\"ok\":true}");
    session.appendAssistant("All good.");

    std::string json;
    ASSERT_TRUE(serializeSession(session, &json));

    EaSessionSnapshot snap;
    ASSERT_TRUE(loadSessionSnapshot(json, &snap));
    EXPECT_EQ(snap.systemPrompt, "You are a test agent.");
    ASSERT_EQ(snap.messages.size(), 4u);

    EaSession restored;
    ASSERT_TRUE(applySessionSnapshot(snap, &restored));
    EXPECT_EQ(restored.systemPrompt(), "You are a test agent.");
    ASSERT_EQ(restored.messages().size(), 5u);
    EXPECT_EQ(restored.messages().back().content, "All good.");
}

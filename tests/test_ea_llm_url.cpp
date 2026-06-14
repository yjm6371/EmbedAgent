#include <gtest/gtest.h>

#include <ea_llm_client.h>

TEST(EaLlmUrlTest, ResolveDeepSeekBase) {
    EXPECT_EQ(embedagent::resolveChatCompletionsUrl("https://api.deepseek.com"),
              "https://api.deepseek.com/chat/completions");
}

TEST(EaLlmUrlTest, ResolveOpenAiBase) {
    EXPECT_EQ(embedagent::resolveChatCompletionsUrl("https://api.openai.com"),
              "https://api.openai.com/v1/chat/completions");
}

TEST(EaLlmUrlTest, PassthroughFullUrl) {
    const std::string full = "https://api.deepseek.com/chat/completions";
    EXPECT_EQ(embedagent::resolveChatCompletionsUrl(full), full);
}

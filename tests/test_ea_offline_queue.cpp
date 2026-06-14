#include <gtest/gtest.h>

#include <ea_file_offline_queue.h>
#include <ea_file_storage.h>
#include <ea_memory_offline_queue.h>
#include <ea_network_error.h>

#include <cstdio>
#include <ctime>
#include <string>

using embedagent::EaFileOfflineQueue;
using embedagent::EaFileStorage;
using embedagent::EaMemoryOfflineQueue;
using embedagent::EaQueuedItem;
using embedagent::isQueueableLlmError;
using embedagent::parseQueuedItem;
using embedagent::serializeQueuedItem;

namespace {

std::string tempDir(const char* name) {
    return std::string("/tmp/ea_test_") + name + "_" +
           std::to_string(static_cast<long>(::time(nullptr)));
}

EaQueuedItem makeItem(const char* id, const char* text) {
    EaQueuedItem item;
    item.id = id;
    item.userText = text;
    item.enqueuedAtMs = 1781246921000LL;
    item.attemptCount = 0;
    return item;
}

}  // namespace

TEST(EaOfflineQueueTest, MemoryFifo) {
    EaMemoryOfflineQueue queue;

    EXPECT_TRUE(queue.enqueue(makeItem("q-1", "hello")));
    EXPECT_TRUE(queue.enqueue(makeItem("q-2", "world")));
    EXPECT_EQ(queue.size(), 2u);

    EaQueuedItem peeked;
    ASSERT_TRUE(queue.peek(&peeked));
    EXPECT_EQ(peeked.id, "q-1");

    EaQueuedItem popped;
    ASSERT_TRUE(queue.dequeue(&popped));
    EXPECT_EQ(popped.userText, "hello");
    EXPECT_EQ(queue.size(), 1u);

    ASSERT_TRUE(queue.removeFront());
    EXPECT_EQ(queue.size(), 0u);
}

TEST(EaOfflineQueueTest, SerializeRoundTrip) {
    EaQueuedItem item = makeItem("q-9", "device status?");
    std::string line;
    ASSERT_TRUE(serializeQueuedItem(item, &line));

    EaQueuedItem parsed;
    ASSERT_TRUE(parseQueuedItem(line, &parsed));
    EXPECT_EQ(parsed.id, item.id);
    EXPECT_EQ(parsed.userText, item.userText);
    EXPECT_EQ(parsed.enqueuedAtMs, item.enqueuedAtMs);
}

TEST(EaOfflineQueueTest, FilePersistenceSurvivesReload) {
    const std::string dir = tempDir("file_queue");
    {
        EaFileOfflineQueue queue(dir);
        EXPECT_TRUE(queue.enqueue(makeItem("q-1", "first")));
        EXPECT_TRUE(queue.enqueue(makeItem("q-2", "second")));
        EXPECT_EQ(queue.size(), 2u);
    }

    EaFileOfflineQueue reloaded(dir);
    EXPECT_EQ(reloaded.size(), 2u);

    EaQueuedItem first;
    ASSERT_TRUE(reloaded.peek(&first));
    EXPECT_EQ(first.userText, "first");

    ASSERT_TRUE(reloaded.removeFront());
    EXPECT_EQ(reloaded.size(), 1u);

    EaFileOfflineQueue reloaded2(dir);
    EXPECT_EQ(reloaded2.size(), 1u);
}

TEST(EaOfflineQueueTest, FileSkipsCorruptLines) {
    const std::string dir = tempDir("corrupt_queue");
    EaFileStorage storage(dir);
    ASSERT_TRUE(storage.ensureDataDir());

    FILE* fp = std::fopen((dir + "/offline_queue.jsonl").c_str(), "w");
    ASSERT_NE(fp, nullptr);
    std::fputs("not-json\n", fp);
    std::string good;
    ASSERT_TRUE(serializeQueuedItem(makeItem("q-1", "ok"), &good));
    std::fputs(good.c_str(), fp);
    std::fputc('\n', fp);
    std::fclose(fp);

    EaFileOfflineQueue queue(dir);
    EXPECT_EQ(queue.size(), 1u);

    EaQueuedItem item;
    ASSERT_TRUE(queue.peek(&item));
    EXPECT_EQ(item.userText, "ok");
}

TEST(EaNetworkErrorTest, QueueableClassification) {
    EXPECT_TRUE(isQueueableLlmError("Could not connect to server"));
    EXPECT_TRUE(isQueueableLlmError("EaLlmClient: HTTP 503"));
    EXPECT_TRUE(isQueueableLlmError("EaLlmClient: HTTP 429"));

    EXPECT_FALSE(isQueueableLlmError(""));
    EXPECT_FALSE(isQueueableLlmError("EaLlmClient: HTTP 401"));
    EXPECT_FALSE(isQueueableLlmError("EaLlmClient: HTTP 400"));
}

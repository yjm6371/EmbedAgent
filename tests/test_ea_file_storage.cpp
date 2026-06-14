#include <gtest/gtest.h>

#include <ea_file_storage.h>
#include <ea_session.h>

#include <ctime>
#include <fstream>

using embedagent::EaFileStorage;
using embedagent::EaSession;

namespace {

std::string tempDir(const char* name) {
    return std::string("/tmp/ea_storage_") + name + "_" +
           std::to_string(static_cast<long>(::time(nullptr)));
}

}  // namespace

TEST(EaFileStorageTest, SessionSaveLoadRoundTrip) {
    const std::string dir = tempDir("session");
    EaFileStorage storage(dir);

    EaSession session;
    session.setSystemPrompt("persist me");
    session.appendUser("hello");
    session.appendAssistant("world");

    ASSERT_TRUE(storage.saveSession(session));

    EaSession loaded;
    ASSERT_TRUE(storage.loadSession(&loaded));
    EXPECT_EQ(loaded.systemPrompt(), "persist me");
    ASSERT_GE(loaded.messages().size(), 2u);
    EXPECT_EQ(loaded.messages().back().content, "world");
}

TEST(EaFileStorageTest, MissingSessionReturnsFalse) {
    const std::string dir = tempDir("missing");
    EaFileStorage storage(dir);
    storage.ensureDataDir();

    EaSession session;
    EXPECT_FALSE(storage.loadSession(&session));
}

TEST(EaFileStorageTest, CorruptSessionReturnsFalse) {
    const std::string dir = tempDir("corrupt");
    EaFileStorage storage(dir);
    storage.ensureDataDir();

    std::ofstream out((dir + "/session.json").c_str());
    out << "not-json";
    out.close();

    EaSession session;
    EXPECT_FALSE(storage.loadSession(&session));
}

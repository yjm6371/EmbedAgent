#include <gtest/gtest.h>

#include <ea_connectivity_monitor.h>
#include <ea_memory_offline_queue.h>
#include <ea_prompt_template.h>
#include <ea_queue_coordinator.h>
#include <ea_session.h>
#include <ea_tool_registry.h>

#include <ev_event_loop.h>
#include <ev_llm_json.h>

#include <memory>
#include <string>

using embedagent::EaAgentOptions;
using embedagent::EaConnectivityMonitor;
using embedagent::EaMemoryOfflineQueue;
using embedagent::EaPromptTemplate;
using embedagent::EaQueueCoordinator;
using embedagent::EaQueueCoordinatorOptions;
using embedagent::EaSession;
using embedagent::EaToolRegistry;

namespace {

std::unique_ptr<EaQueueCoordinator> makeCoordinator(
    cppev::EvEventLoop* loop,
    EaAgentOptions opts,
    EaQueueCoordinatorOptions coordOpts) {
    std::unique_ptr<EaMemoryOfflineQueue> queue(new EaMemoryOfflineQueue());
    std::unique_ptr<EaConnectivityMonitor> monitor(new EaConnectivityMonitor(loop));
    return std::unique_ptr<EaQueueCoordinator>(
        new EaQueueCoordinator(loop, opts, coordOpts,
                               std::move(queue), std::move(monitor)));
}

EaAgentOptions mockAgentOptions() {
    EaAgentOptions opts;
    opts.llm.model = "mock-model";
    opts.llm.stream = false;
    opts.llm.mockResponses.push_back(
        "{\"choices\":[{\"message\":{\"role\":\"assistant\","
        "\"content\":\"Queued reply.\"},\"finish_reason\":\"stop\"}]}");

    EaPromptTemplate tmpl;
    tmpl.setTemplate("You are {{device_name}}.");
    opts.systemTemplate = tmpl;
    opts.deviceName = "queue-test";
    return opts;
}

}  // namespace

TEST(EaQueueCoordinatorTest, OfflineSubmitQueuesWithoutSessionUser) {
    cppev::EvEventLoop loop;

    EaQueueCoordinatorOptions coordOpts;
    coordOpts.maxQueueItems = 4;

    std::unique_ptr<EaQueueCoordinator> coord =
        makeCoordinator(&loop, mockAgentOptions(), coordOpts);

    coord->connectivity().setOnline(false);

    bool done = false;
    bool ok = false;
    std::string error;

    coord->submitUserMessage(
        "offline hello",
        [](const std::string& /*delta*/) {},
        [&done, &ok, &error](bool success, const std::string& err) {
            done = true;
            ok = success;
            error = err;
        });

    ASSERT_TRUE(done);
    EXPECT_TRUE(ok);
    EXPECT_EQ(error, "queued");
    EXPECT_EQ(coord->pendingCount(), 1u);

    bool hasUser = false;
    const std::vector<cppev::ext::EvLlmMessage>& msgs =
        coord->agent().session().messages();
    for (std::size_t i = 0; i < msgs.size(); ++i) {
        if (msgs[i].role == "user") {
            hasUser = true;
            break;
        }
    }
    EXPECT_FALSE(hasUser);
}

TEST(EaQueueCoordinatorTest, FlushMockDeliversAssistantReply) {
    cppev::EvEventLoop loop;

    EaQueueCoordinatorOptions coordOpts;
    coordOpts.autoFlushOnOnline = false;

    std::unique_ptr<EaQueueCoordinator> coord =
        makeCoordinator(&loop, mockAgentOptions(), coordOpts);

    coord->connectivity().setOnline(false);
    coord->submitUserMessage(
        "flush me",
        [](const std::string& /*delta*/) {},
        [](bool, const std::string&) {});

    EXPECT_EQ(coord->pendingCount(), 1u);

    coord->connectivity().setOnline(true);

    bool done = false;
    bool ok = false;
    std::string streamed;
    std::string error;

    coord->flush(
        [&streamed](const std::string& delta) { streamed += delta; },
        [&done, &ok, &error](bool success, const std::string& err) {
            done = true;
            ok = success;
            error = err;
        });

    ASSERT_TRUE(done);
    EXPECT_TRUE(ok) << error;
    EXPECT_EQ(streamed, "Queued reply.");
    EXPECT_EQ(coord->pendingCount(), 0u);
    ASSERT_FALSE(coord->agent().session().messages().empty());
    EXPECT_EQ(coord->agent().session().messages().back().role, "assistant");
}

TEST(EaQueueCoordinatorTest, QueueFullRejects) {
    cppev::EvEventLoop loop;

    EaQueueCoordinatorOptions coordOpts;
    coordOpts.maxQueueItems = 1;

    std::unique_ptr<EaQueueCoordinator> coord =
        makeCoordinator(&loop, mockAgentOptions(), coordOpts);

    coord->connectivity().setOnline(false);

    bool firstOk = false;
    coord->submitUserMessage(
        "one",
        [](const std::string& /*delta*/) {},
        [&firstOk](bool ok, const std::string&) { firstOk = ok; });
    EXPECT_TRUE(firstOk);

    bool secondDone = false;
    bool secondOk = true;
    coord->submitUserMessage(
        "two",
        [](const std::string& /*delta*/) {},
        [&secondDone, &secondOk](bool ok, const std::string&) {
            secondDone = true;
            secondOk = ok;
        });

    ASSERT_TRUE(secondDone);
    EXPECT_FALSE(secondOk);
    EXPECT_EQ(coord->pendingCount(), 1u);
}

TEST(EaSessionTest, RollbackLastUser) {
    EaSession session;
    session.appendUser("hello");
    ASSERT_TRUE(session.rollbackLastUser());
    EXPECT_TRUE(session.messages().empty());

    session.appendUser("again");
    session.appendAssistant("reply");
    EXPECT_FALSE(session.rollbackLastUser());
    EXPECT_EQ(session.messages().size(), 2u);
}

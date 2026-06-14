// ea_queue_coordinator.h — offline queue + agent orchestration
#pragma once

#include <ea_agent.h>
#include <ea_connectivity_monitor.h>
#include <ea_offline_queue.h>

#include <cstddef>
#include <memory>
#include <string>

namespace embedagent {

struct EaQueueCoordinatorOptions {
    int  maxQueueItems     {100};
    int  maxItemAttempts   {3};
    bool autoFlushOnOnline {true};
};

using EaQueueCoordinatorTextCallback = EaAgentTextCallback;
using EaQueueCoordinatorDoneCallback = EaAgentDoneCallback;

// call in loop thread only
class EaQueueCoordinator {
public:
    EaQueueCoordinator(cppev::EvEventLoop* loop,
                       const EaAgentOptions& agentOpts,
                       const EaQueueCoordinatorOptions& coordOpts,
                       std::unique_ptr<EaOfflineQueue> queue,
                       std::unique_ptr<EaConnectivityMonitor> monitor);

    ~EaQueueCoordinator();

    EaQueueCoordinator(const EaQueueCoordinator&)            = delete;
    EaQueueCoordinator& operator=(const EaQueueCoordinator&) = delete;

    void setToolRegistry(EaToolRegistry* registry);
    void setRoundStartCallback(EaAgentRoundCallback cb);

    void submitUserMessage(const std::string& text,
                           EaQueueCoordinatorTextCallback onText,
                           EaQueueCoordinatorDoneCallback onDone);

    void flush(EaQueueCoordinatorTextCallback onText,
               EaQueueCoordinatorDoneCallback onDone);

    void cancel();
    std::size_t pendingCount() const;

    EaAgent& agent();
    EaOfflineQueue& queue();
    EaConnectivityMonitor& connectivity();

private:
    struct FlushContext {
        EaQueueCoordinatorTextCallback onText;
        EaQueueCoordinatorDoneCallback onDone;
    };

    std::string nextQueueId();
    bool enqueueUserText(const std::string& text);
    void tryFlush();
    void onConnectivityChanged(EaConnectivityState state);
    void finishFlushItem(bool ok, const std::string& error);

    cppev::EvEventLoop*                     loop_;
    EaAgentOptions                          agentOpts_;
    EaQueueCoordinatorOptions               coordOpts_;
    std::unique_ptr<EaAgent>              agent_;
    std::unique_ptr<EaOfflineQueue>       queue_;
    std::unique_ptr<EaConnectivityMonitor> monitor_;
    EaToolRegistry*                         registry_ {nullptr};
    bool                                    flushing_ {false};
    std::shared_ptr<FlushContext>           flushCtx_;
    int                                     queueSerial_ {0};
};

}  // namespace embedagent

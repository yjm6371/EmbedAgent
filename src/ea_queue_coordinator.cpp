#include <ea_queue_coordinator.h>

#include <ea_network_error.h>

#include <ev_logger.h>

#include <sstream>

namespace embedagent {

namespace {

const char* kQueuedMessage = "queued";

}  // namespace

EaQueueCoordinator::EaQueueCoordinator(
    cppev::EvEventLoop* loop,
    const EaAgentOptions& agentOpts,
    const EaQueueCoordinatorOptions& coordOpts,
    std::unique_ptr<EaOfflineQueue> queue,
    std::unique_ptr<EaConnectivityMonitor> monitor)
    : loop_(loop)
    , agentOpts_(agentOpts)
    , coordOpts_(coordOpts)
    , agent_(new EaAgent(loop_, agentOpts_))
    , queue_(queue.release())
    , monitor_(monitor.release())
{
    queue_->load();

    EaQueueCoordinator* self = this;
    monitor_->setTransitionCallback([self](EaConnectivityState state) {
        self->onConnectivityChanged(state);
    });
}

EaQueueCoordinator::~EaQueueCoordinator() {
    cancel();
}

void EaQueueCoordinator::setToolRegistry(EaToolRegistry* registry) {
    registry_ = registry;
    agent_->setToolRegistry(registry_);
}

void EaQueueCoordinator::setRoundStartCallback(EaAgentRoundCallback cb) {
    agent_->setRoundStartCallback(cb);
}

EaAgent& EaQueueCoordinator::agent() {
    return *agent_;
}

EaOfflineQueue& EaQueueCoordinator::queue() {
    return *queue_;
}

EaConnectivityMonitor& EaQueueCoordinator::connectivity() {
    return *monitor_;
}

std::size_t EaQueueCoordinator::pendingCount() const {
    return queue_->size();
}

std::string EaQueueCoordinator::nextQueueId() {
    std::ostringstream ss;
    ss << "q-" << ++queueSerial_;
    return ss.str();
}

bool EaQueueCoordinator::enqueueUserText(const std::string& text) {
    if (queue_->size() >= static_cast<std::size_t>(coordOpts_.maxQueueItems)) {
        return false;
    }

    EaQueuedItem item;
    item.id = nextQueueId();
    item.userText = text;
    item.enqueuedAtMs = currentTimeMs();
    item.attemptCount = 0;
    return queue_->enqueue(item);
}

void EaQueueCoordinator::onConnectivityChanged(EaConnectivityState state) {
    if (state == EaConnectivityState::kOnline &&
        coordOpts_.autoFlushOnOnline) {
        tryFlush();
    }
}

void EaQueueCoordinator::submitUserMessage(
    const std::string& text,
    EaQueueCoordinatorTextCallback onText,
    EaQueueCoordinatorDoneCallback onDone) {
    if (!monitor_->isOnline()) {
        if (!enqueueUserText(text)) {
            if (onDone) {
                onDone(false, "EaQueueCoordinator: queue full");
            }
            return;
        }

        LOGI("EaQueueCoordinator: queued offline user message");
        if (onDone) {
            onDone(true, kQueuedMessage);
        }
        return;
    }

    if (flushing_) {
        if (!enqueueUserText(text)) {
            if (onDone) {
                onDone(false, "EaQueueCoordinator: queue full");
            }
            return;
        }

        if (onDone) {
            onDone(true, kQueuedMessage);
        }
        return;
    }

    agent_->submitUserMessage(
        text,
        onText,
        [this, text, onDone](bool ok, const std::string& error) {
            if (ok) {
                monitor_->setOnline(true);
                if (onDone) {
                    onDone(true, std::string());
                }
                return;
            }

            if (!isQueueableLlmError(error)) {
                agent_->session().rollbackLastUser();
                if (onDone) {
                    onDone(false, error);
                }
                return;
            }

            monitor_->setOnline(false);
            agent_->session().rollbackLastUser();

            if (!enqueueUserText(text)) {
                if (onDone) {
                    onDone(false, "EaQueueCoordinator: queue full");
                }
                return;
            }

            LOGI("EaQueueCoordinator: queued after network failure");
            if (onDone) {
                onDone(true, kQueuedMessage);
            }
        });
}

void EaQueueCoordinator::flush(EaQueueCoordinatorTextCallback onText,
                             EaQueueCoordinatorDoneCallback onDone) {
    flushCtx_.reset(new FlushContext());
    flushCtx_->onText = onText;
    flushCtx_->onDone = onDone;
    tryFlush();
}

void EaQueueCoordinator::tryFlush() {
    if (!monitor_->isOnline()) {
        return;
    }

    if (flushing_) {
        return;
    }

    if (queue_->size() == 0) {
        if (flushCtx_ && flushCtx_->onDone) {
            flushCtx_->onDone(true, std::string());
        }
        flushCtx_.reset();
        return;
    }

    EaQueuedItem item;
    if (!queue_->peek(&item)) {
        return;
    }

    if (item.attemptCount >= coordOpts_.maxItemAttempts) {
        LOGE("EaQueueCoordinator: dropping item %s after max attempts",
             item.id.c_str());
        queue_->removeFront();
        tryFlush();
        return;
    }

    flushing_ = true;
    LOGI("EaQueueCoordinator: flushing item %s", item.id.c_str());

    std::shared_ptr<FlushContext> ctx = flushCtx_;
    agent_->submitUserMessage(
        item.userText,
        [ctx](const std::string& delta) {
            if (ctx && ctx->onText) {
                ctx->onText(delta);
            }
        },
        [this, item, ctx](bool ok, const std::string& error) {
            finishFlushItem(ok, error);
            (void)item;
            (void)ctx;
        });
}

void EaQueueCoordinator::finishFlushItem(bool ok, const std::string& error) {
    flushing_ = false;

    if (ok) {
        monitor_->setOnline(true);
        queue_->removeFront();
        tryFlush();
        return;
    }

    if (!isQueueableLlmError(error)) {
        LOGE("EaQueueCoordinator: flush failed (non-retryable): %s",
             error.c_str());
        queue_->removeFront();
        tryFlush();
        return;
    }

    monitor_->setOnline(false);
    agent_->session().rollbackLastUser();

    EaQueuedItem front;
    if (queue_->peek(&front)) {
        queue_->updateFrontAttempt(front.attemptCount + 1);
    }

    EaQueueCoordinatorDoneCallback doneCb;
    if (flushCtx_) {
        doneCb = flushCtx_->onDone;
        flushCtx_.reset();
    }

    if (doneCb) {
        doneCb(false, error);
    }
}

void EaQueueCoordinator::cancel() {
    agent_->cancel();
    flushing_ = false;
    flushCtx_.reset();
}

}  // namespace embedagent

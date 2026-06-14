#include <ea_connectivity_monitor.h>

#include <ev_logger.h>

namespace embedagent {

EaConnectivityMonitor::EaConnectivityMonitor(cppev::EvEventLoop* loop)
    : loop_(loop)
    , probeClient_(new cppev::ext::EvHttpClient(loop_))
{}

EaConnectivityMonitor::~EaConnectivityMonitor() {
    stopProbe();
}

void EaConnectivityMonitor::setOnline(bool online) {
    transitionTo(online ? EaConnectivityState::kOnline
                        : EaConnectivityState::kOffline);
}

bool EaConnectivityMonitor::isOnline() const {
    return state_ != EaConnectivityState::kOffline;
}

void EaConnectivityMonitor::setTransitionCallback(EaConnectivityCallback cb) {
    onTransition_ = cb;
}

void EaConnectivityMonitor::transitionTo(EaConnectivityState state) {
    if (state_ == state) {
        return;
    }

    state_ = state;
    LOGI("EaConnectivityMonitor: state -> %s",
         state == EaConnectivityState::kOnline ? "online" :
         state == EaConnectivityState::kOffline ? "offline" : "unknown");

    if (onTransition_) {
        onTransition_(state_);
    }
}

void EaConnectivityMonitor::startProbe(const std::string& probeUrl,
                                       double intervalSec) {
    stopProbe();

    probeUrl_ = probeUrl;
    probeIntervalSec_ = intervalSec > 0.0 ? intervalSec : 30.0;

    if (probeUrl_.empty()) {
        return;
    }

    EaConnectivityMonitor* self = this;
    probeTimer_ = loop_->runEvery(probeIntervalSec_, [self]() {
        self->runProbe();
    });

    runProbe();
}

void EaConnectivityMonitor::stopProbe() {
    loop_->cancelTimer(probeTimer_);
    probeInFlight_ = false;
}

void EaConnectivityMonitor::runProbe() {
    if (probeUrl_.empty() || probeInFlight_) {
        return;
    }

    probeInFlight_ = true;

    cppev::ext::EvHttpRequest req;
    req.method = "GET";
    req.url = probeUrl_;
    req.totalTimeoutSec = 10;
    req.connectTimeoutSec = 5;

    EaConnectivityMonitor* self = this;
    probeClient_->request(req, [self](const std::string& /*url*/, int code,
                                      const std::string& /*body*/,
                                      const std::string& err) {
        self->probeInFlight_ = false;

        if (!err.empty() || code < 200 || code >= 400) {
            self->transitionTo(EaConnectivityState::kOffline);
            return;
        }

        self->transitionTo(EaConnectivityState::kOnline);
    });
}

}  // namespace embedagent

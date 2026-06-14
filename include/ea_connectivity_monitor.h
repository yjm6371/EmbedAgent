// ea_connectivity_monitor.h — online/offline state with optional HTTP probe
#pragma once

#include <ev_event_loop.h>
#include <ev_http_client.h>
#include <ev_timer_id.h>

#include <functional>
#include <memory>
#include <string>

namespace embedagent {

enum class EaConnectivityState {
    kOnline,
    kOffline,
    kUnknown
};

using EaConnectivityCallback =
    std::function<void(EaConnectivityState state)>;

// call in loop thread only
class EaConnectivityMonitor {
public:
    explicit EaConnectivityMonitor(cppev::EvEventLoop* loop);
    ~EaConnectivityMonitor();

    EaConnectivityMonitor(const EaConnectivityMonitor&)            = delete;
    EaConnectivityMonitor& operator=(const EaConnectivityMonitor&) = delete;

    void setOnline(bool online);
    bool isOnline() const;
    EaConnectivityState state() const { return state_; }

    void setTransitionCallback(EaConnectivityCallback cb);

    void startProbe(const std::string& probeUrl, double intervalSec);
    void stopProbe();

private:
    void transitionTo(EaConnectivityState state);
    void runProbe();

    cppev::EvEventLoop*                        loop_;
    EaConnectivityState                          state_ {EaConnectivityState::kUnknown};
    EaConnectivityCallback                       onTransition_;
    std::unique_ptr<cppev::ext::EvHttpClient>  probeClient_;
    cppev::EvTimerId                             probeTimer_;
    std::string                                  probeUrl_;
    double                                       probeIntervalSec_ {30.0};
    bool                                         probeInFlight_ {false};
};

}  // namespace embedagent

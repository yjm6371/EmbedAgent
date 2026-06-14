// ea_memory_offline_queue.h — in-memory FIFO offline queue
#pragma once

#include <ea_offline_queue.h>

#include <deque>

namespace embedagent {

class EaMemoryOfflineQueue : public EaOfflineQueue {
public:
    bool enqueue(const EaQueuedItem& item) override;
    bool peek(EaQueuedItem* out) const override;
    bool dequeue(EaQueuedItem* out) override;
    bool removeFront() override;
    bool updateFrontAttempt(int attemptCount) override;
    std::size_t size() const override;
    bool load() override;

private:
    std::deque<EaQueuedItem> items_;
};

}  // namespace embedagent

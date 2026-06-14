#include <ea_memory_offline_queue.h>

namespace embedagent {

bool EaMemoryOfflineQueue::enqueue(const EaQueuedItem& item) {
    items_.push_back(item);
    return true;
}

bool EaMemoryOfflineQueue::peek(EaQueuedItem* out) const {
    if (!out || items_.empty()) {
        return false;
    }
    *out = items_.front();
    return true;
}

bool EaMemoryOfflineQueue::dequeue(EaQueuedItem* out) {
    if (items_.empty()) {
        return false;
    }
    if (out) {
        *out = items_.front();
    }
    items_.pop_front();
    return true;
}

bool EaMemoryOfflineQueue::removeFront() {
    if (items_.empty()) {
        return false;
    }
    items_.pop_front();
    return true;
}

bool EaMemoryOfflineQueue::updateFrontAttempt(int attemptCount) {
    if (items_.empty()) {
        return false;
    }
    items_.front().attemptCount = attemptCount;
    return true;
}

std::size_t EaMemoryOfflineQueue::size() const {
    return items_.size();
}

bool EaMemoryOfflineQueue::load() {
    return true;
}

}  // namespace embedagent

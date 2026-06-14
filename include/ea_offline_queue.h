// ea_offline_queue.h — offline user-turn queue (memory or file-backed)
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace embedagent {

struct EaQueuedItem {
    std::string id;
    std::string userText;
    int64_t     enqueuedAtMs {0};
    int         attemptCount {0};
};

class EaOfflineQueue {
public:
    virtual ~EaOfflineQueue() {}

    virtual bool enqueue(const EaQueuedItem& item) = 0;
    virtual bool peek(EaQueuedItem* out) const = 0;
    virtual bool dequeue(EaQueuedItem* out) = 0;
    virtual bool removeFront() = 0;
    virtual bool updateFrontAttempt(int attemptCount) = 0;
    virtual std::size_t size() const = 0;
    virtual bool load() = 0;
};

// Serialize / deserialize one JSONL record.
bool serializeQueuedItem(const EaQueuedItem& item, std::string* out);
bool parseQueuedItem(const std::string& line, EaQueuedItem* out);

int64_t currentTimeMs();

}  // namespace embedagent

// ea_file_offline_queue.h — JSONL-persisted offline queue
#pragma once

#include <ea_file_storage.h>
#include <ea_offline_queue.h>

#include <deque>
#include <string>

namespace embedagent {

class EaFileOfflineQueue : public EaOfflineQueue {
public:
    explicit EaFileOfflineQueue(const std::string& dataDir);

    bool enqueue(const EaQueuedItem& item) override;
    bool peek(EaQueuedItem* out) const override;
    bool dequeue(EaQueuedItem* out) override;
    bool removeFront() override;
    bool updateFrontAttempt(int attemptCount) override;
    std::size_t size() const override;
    bool load() override;

private:
    bool persistAll() const;
    bool appendLine(const EaQueuedItem& item);
    std::string queuePath() const;

    EaFileStorage           storage_;
    std::deque<EaQueuedItem> items_;
};

}  // namespace embedagent

#include <ea_file_offline_queue.h>

#include <ev_logger.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace embedagent {

EaFileOfflineQueue::EaFileOfflineQueue(const std::string& dataDir)
    : storage_(dataDir)
{
    load();
}

std::string EaFileOfflineQueue::queuePath() const {
    return storage_.dataDir() + "/offline_queue.jsonl";
}

bool EaFileOfflineQueue::load() {
    items_.clear();

    if (!storage_.ensureDataDir()) {
        return false;
    }

    std::ifstream in(queuePath().c_str());
    if (!in) {
        return true;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }

        EaQueuedItem item;
        if (!parseQueuedItem(line, &item)) {
            LOGW("EaFileOfflineQueue: skipping corrupt line");
            continue;
        }
        items_.push_back(item);
    }

    return true;
}

bool EaFileOfflineQueue::appendLine(const EaQueuedItem& item) {
    std::string line;
    if (!serializeQueuedItem(item, &line)) {
        return false;
    }

    if (!storage_.ensureDataDir()) {
        return false;
    }

    FILE* fp = std::fopen(queuePath().c_str(), "a");
    if (!fp) {
        LOGE("EaFileOfflineQueue: failed to append %s", queuePath().c_str());
        return false;
    }

    std::fputs(line.c_str(), fp);
    std::fputc('\n', fp);
    std::fflush(fp);
    std::fclose(fp);
    return true;
}

bool EaFileOfflineQueue::persistAll() const {
    if (!storage_.ensureDataDir()) {
        return false;
    }

    const std::string path = queuePath();
    const std::string tmpPath = path + ".tmp";

    std::ofstream out(tmpPath.c_str(), std::ios::trunc);
    if (!out) {
        LOGE("EaFileOfflineQueue: failed to rewrite %s", tmpPath.c_str());
        return false;
    }

    for (std::size_t i = 0; i < items_.size(); ++i) {
        std::string line;
        if (!serializeQueuedItem(items_[i], &line)) {
            return false;
        }
        out << line << '\n';
    }

    out.flush();
    if (!out.good()) {
        return false;
    }
    out.close();

    if (std::rename(tmpPath.c_str(), path.c_str()) != 0) {
        LOGE("EaFileOfflineQueue: rename failed for %s", path.c_str());
        return false;
    }

    return true;
}

bool EaFileOfflineQueue::enqueue(const EaQueuedItem& item) {
    items_.push_back(item);
    return appendLine(item);
}

bool EaFileOfflineQueue::peek(EaQueuedItem* out) const {
    if (!out || items_.empty()) {
        return false;
    }
    *out = items_.front();
    return true;
}

bool EaFileOfflineQueue::dequeue(EaQueuedItem* out) {
    if (items_.empty()) {
        return false;
    }
    if (out) {
        *out = items_.front();
    }
    items_.pop_front();
    return persistAll();
}

bool EaFileOfflineQueue::removeFront() {
    if (items_.empty()) {
        return false;
    }
    items_.pop_front();
    return persistAll();
}

bool EaFileOfflineQueue::updateFrontAttempt(int attemptCount) {
    if (items_.empty()) {
        return false;
    }
    items_.front().attemptCount = attemptCount;
    return persistAll();
}

std::size_t EaFileOfflineQueue::size() const {
    return items_.size();
}

}  // namespace embedagent

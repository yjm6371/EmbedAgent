#include <ea_offline_queue.h>

#include <ev_json.h>

#include <chrono>
#include <sstream>

namespace embedagent {

int64_t currentTimeMs() {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    using std::chrono::system_clock;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
        .count();
}

bool serializeQueuedItem(const EaQueuedItem& item, std::string* out) {
    if (!out) {
        return false;
    }

    cppev::ext::EvJsonBuilder builder;
    builder.beginObject();
    builder.addString("id", item.id);
    builder.addString("userText", item.userText);
    builder.addString("enqueuedAtMs", std::to_string(item.enqueuedAtMs));
    builder.addInt("attemptCount", item.attemptCount);
    builder.endObject();
    return builder.finish(out);
}

bool parseQueuedItem(const std::string& line, EaQueuedItem* out) {
    if (!out || line.empty()) {
        return false;
    }

    cppev::ext::EvJsonDocument doc;
    if (!cppev::ext::EvJsonDocument::parse(line, &doc)) {
        return false;
    }

    if (!doc.getString("id", &out->id) ||
        !doc.getString("userText", &out->userText)) {
        return false;
    }

    std::string enqueuedStr;
    if (!doc.getString("enqueuedAtMs", &enqueuedStr)) {
        return false;
    }
    out->enqueuedAtMs = std::stoll(enqueuedStr);

    if (!doc.getInt("attemptCount", &out->attemptCount)) {
        out->attemptCount = 0;
    }

    return true;
}

}  // namespace embedagent

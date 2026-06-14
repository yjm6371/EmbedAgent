#include <ea_network_error.h>

#include <cstdlib>

namespace embedagent {

namespace {

int parseHttpCodeFromError(const std::string& error) {
    const std::string prefix = "EaLlmClient: HTTP ";
    const std::size_t pos = error.find(prefix);
    if (pos == std::string::npos) {
        return 0;
    }

    const std::size_t start = pos + prefix.size();
    const std::size_t end = error.find_first_not_of("0123456789", start);
    const std::string digits = error.substr(
        start, end == std::string::npos ? std::string::npos : end - start);
    if (digits.empty()) {
        return 0;
    }

    return std::atoi(digits.c_str());
}

}  // namespace

bool isQueueableLlmError(const std::string& error) {
    if (error.empty()) {
        return false;
    }

    const int httpCode = parseHttpCodeFromError(error);
    if (httpCode == 0) {
        // libcurl transport error or other non-HTTP failure.
        return true;
    }

    if (httpCode >= 500) {
        return true;
    }

    if (httpCode == 429) {
        return true;
    }

    return false;
}

}  // namespace embedagent

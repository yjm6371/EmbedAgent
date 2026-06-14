#include <ea_tool_args.h>

#include <ev_json.h>

namespace embedagent {

EaToolArgs::EaToolArgs(const std::string& argsJson)
    : valid_(false)
    , doc_(new cppev::ext::EvJsonDocument())
{
    if (!argsJson.empty()) {
        valid_ = cppev::ext::EvJsonDocument::parse(argsJson, doc_.get());
    }
}

EaToolArgs::~EaToolArgs() {}

std::string EaToolArgs::getString(const std::string& key,
                                  const std::string& def) const {
    if (!valid_) {
        return def;
    }
    std::string val;
    if (!doc_->getString(key.c_str(), &val)) {
        return def;
    }
    return val;
}

int EaToolArgs::getInt(const std::string& key, int def) const {
    if (!valid_) {
        return def;
    }
    int val = def;
    if (!doc_->getInt(key.c_str(), &val)) {
        return def;
    }
    return val;
}

double EaToolArgs::getDouble(const std::string& key, double def) const {
    if (!valid_) {
        return def;
    }
    // EvJsonDocument only exposes numbers as int; widen to double.
    int val = static_cast<int>(def);
    if (!doc_->getInt(key.c_str(), &val)) {
        return def;
    }
    return static_cast<double>(val);
}

bool EaToolArgs::getBool(const std::string& key, bool def) const {
    if (!valid_) {
        return def;
    }
    bool val = def;
    if (!doc_->getBool(key.c_str(), &val)) {
        return def;
    }
    return val;
}

bool EaToolArgs::has(const std::string& key) const {
    if (!valid_) {
        return false;
    }
    // Probe each supported primitive type; return true on first match.
    std::string s;
    if (doc_->getString(key.c_str(), &s)) {
        return true;
    }
    int i = 0;
    if (doc_->getInt(key.c_str(), &i)) {
        return true;
    }
    bool b = false;
    if (doc_->getBool(key.c_str(), &b)) {
        return true;
    }
    // Nested objects/arrays count as present even if not a primitive.
    cppev::ext::EvJsonDocument sub;
    if (doc_->getObject(key.c_str(), &sub)) {
        return true;
    }
    return false;
}

}  // namespace embedagent

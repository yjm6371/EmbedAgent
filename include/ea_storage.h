// ea_storage.h — abstract interface for session and key-value persistence
#pragma once

#include <ea_session.h>

#include <string>

namespace embedagent {

class EaStorage {
public:
    virtual ~EaStorage() {}

    virtual bool loadSession(EaSession* session) const = 0;
    virtual bool saveSession(const EaSession& session) = 0;

    virtual bool getString(const std::string& key, std::string* out) const = 0;
    virtual bool putString(const std::string& key, const std::string& value) = 0;
};

}  // namespace embedagent

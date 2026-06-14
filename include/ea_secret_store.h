// ea_secret_store.h — API key storage interface
#pragma once

#include <string>

namespace embedagent {

enum class EaSecretSource {
    kNone,
    kCli,
    kEnv,
    kFile
};

class EaSecretStore {
public:
    virtual ~EaSecretStore() {}

    virtual bool loadApiKey(std::string* out) const = 0;
    virtual bool saveApiKey(const std::string& key) = 0;
    virtual bool hasApiKey() const = 0;
    virtual bool deleteApiKey() = 0;
};

}  // namespace embedagent

// ea_env_secret_store.h — read API key from environment variable
#pragma once

#include <ea_secret_store.h>

namespace embedagent {

class EaEnvSecretStore : public EaSecretStore {
public:
    explicit EaEnvSecretStore(const char* envVar = "EMBEDAGENT_API_KEY");

    bool loadApiKey(std::string* out) const override;
    bool saveApiKey(const std::string& /*key*/) override;
    bool hasApiKey() const override;
    bool deleteApiKey() override;

private:
    const char* envVar_;
};

}  // namespace embedagent

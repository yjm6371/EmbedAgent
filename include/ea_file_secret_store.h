// ea_file_secret_store.h — obfuscated API key file store
#pragma once

#include <ea_secret_store.h>

#include <string>

namespace embedagent {

class EaFileSecretStore : public EaSecretStore {
public:
    explicit EaFileSecretStore(const std::string& dataDir);

    bool loadApiKey(std::string* out) const override;
    bool saveApiKey(const std::string& key) override;
    bool hasApiKey() const override;
    bool deleteApiKey() override;

private:
    bool ensureSecretsDir() const;
    std::string secretPath() const;

    std::string dataDir_;
};

}  // namespace embedagent

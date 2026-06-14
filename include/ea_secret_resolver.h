// ea_secret_resolver.h — resolve API key from CLI / env / file
#pragma once

#include <ea_secret_store.h>

#include <memory>
#include <string>

namespace embedagent {

class EaSecretResolver {
public:
    explicit EaSecretResolver(const std::string& dataDir);

    void setCliOverride(const std::string& key);
    bool resolveApiKey(std::string* out,
                       EaSecretSource* source = nullptr) const;
    bool saveApiKeyToFile(const std::string& key);

private:
    std::string                    cliOverride_;
    std::unique_ptr<EaSecretStore> envStore_;
    std::unique_ptr<EaSecretStore> fileStore_;
};

}  // namespace embedagent

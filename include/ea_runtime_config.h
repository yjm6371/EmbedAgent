// ea_runtime_config.h — bootstrap dataDir secrets and session persistence
#pragma once

#include <ea_file_storage.h>
#include <ea_secret_resolver.h>
#include <ea_secret_store.h>

#include <string>

namespace embedagent {

struct EaRuntimeConfigOptions {
    std::string dataDir;
    std::string cliApiKey;
    bool        saveApiKey {false};
    bool        persistSession {false};
};

class EaRuntimeConfig {
public:
    EaFileStorage    storage;
    EaSecretResolver secrets;
    bool             persistSession {false};

    explicit EaRuntimeConfig(const EaRuntimeConfigOptions& opts);

    bool resolveApiKey(std::string* out,
                       EaSecretSource* source = nullptr) const;
};

}  // namespace embedagent

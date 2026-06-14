#include <ea_env_secret_store.h>

#include <cstdlib>

namespace embedagent {

EaEnvSecretStore::EaEnvSecretStore(const char* envVar)
    : envVar_(envVar ? envVar : "EMBEDAGENT_API_KEY")
{}

bool EaEnvSecretStore::loadApiKey(std::string* out) const {
    if (!out) {
        return false;
    }

    const char* value = std::getenv(envVar_);
    if (!value || value[0] == '\0') {
        return false;
    }

    *out = value;
    return true;
}

bool EaEnvSecretStore::saveApiKey(const std::string& /*key*/) {
    return false;
}

bool EaEnvSecretStore::hasApiKey() const {
    const char* value = std::getenv(envVar_);
    return value != nullptr && value[0] != '\0';
}

bool EaEnvSecretStore::deleteApiKey() {
    return false;
}

}  // namespace embedagent

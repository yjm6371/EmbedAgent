#include <ea_secret_resolver.h>

#include <ea_env_secret_store.h>
#include <ea_file_secret_store.h>

namespace embedagent {

EaSecretResolver::EaSecretResolver(const std::string& dataDir)
    : envStore_(new EaEnvSecretStore())
    , fileStore_(new EaFileSecretStore(dataDir))
{}

void EaSecretResolver::setCliOverride(const std::string& key) {
    cliOverride_ = key;
}

bool EaSecretResolver::resolveApiKey(std::string* out,
                                     EaSecretSource* source) const {
    if (!out) {
        return false;
    }

    out->clear();

    if (!cliOverride_.empty()) {
        *out = cliOverride_;
        if (source) {
            *source = EaSecretSource::kCli;
        }
        return true;
    }

    if (envStore_->loadApiKey(out)) {
        if (source) {
            *source = EaSecretSource::kEnv;
        }
        return true;
    }

    if (fileStore_->loadApiKey(out)) {
        if (source) {
            *source = EaSecretSource::kFile;
        }
        return true;
    }

    if (source) {
        *source = EaSecretSource::kNone;
    }
    return false;
}

bool EaSecretResolver::saveApiKeyToFile(const std::string& key) {
    return fileStore_->saveApiKey(key);
}

}  // namespace embedagent

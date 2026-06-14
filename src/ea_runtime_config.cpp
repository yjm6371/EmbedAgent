#include <ea_runtime_config.h>

#include <ev_logger.h>

namespace embedagent {

EaRuntimeConfig::EaRuntimeConfig(const EaRuntimeConfigOptions& opts)
    : storage(opts.dataDir)
    , secrets(opts.dataDir)
    , persistSession(opts.persistSession)
{
    if (!opts.cliApiKey.empty()) {
        secrets.setCliOverride(opts.cliApiKey);
    }

    if (opts.saveApiKey && !opts.cliApiKey.empty()) {
        if (secrets.saveApiKeyToFile(opts.cliApiKey)) {
            LOGI("EaRuntimeConfig: saved API key to file store");
        } else {
            LOGW("EaRuntimeConfig: failed to save API key");
        }
    }

    storage.ensureDataDir();
}

bool EaRuntimeConfig::resolveApiKey(std::string* out,
                                    EaSecretSource* source) const {
    return secrets.resolveApiKey(out, source);
}

}  // namespace embedagent

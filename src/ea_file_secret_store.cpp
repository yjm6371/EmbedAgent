#include <ea_file_secret_store.h>

#include <ea_file_storage.h>
#include <ea_secret_obfuscator.h>

#include <ev_logger.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace embedagent {

EaFileSecretStore::EaFileSecretStore(const std::string& dataDir)
    : dataDir_(dataDir)
{}

std::string EaFileSecretStore::secretPath() const {
    return dataDir_ + "/secrets/api_key.enc";
}

bool EaFileSecretStore::ensureSecretsDir() const {
    EaFileStorage storage(dataDir_);
    if (!storage.ensureDataDir()) {
        return false;
    }

    const std::string secretsDir = dataDir_ + "/secrets";
    struct stat st;
    if (::stat(secretsDir.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }

    if (::mkdir(secretsDir.c_str(), S_IRWXU) != 0) {
        LOGE("EaFileSecretStore: mkdir %s failed: %s",
             secretsDir.c_str(), std::strerror(errno));
        return false;
    }

    return true;
}

bool EaFileSecretStore::hasApiKey() const {
    struct stat st;
    return ::stat(secretPath().c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool EaFileSecretStore::loadApiKey(std::string* out) const {
    if (!out || !hasApiKey()) {
        return false;
    }

    std::ifstream in(secretPath().c_str());
    if (!in) {
        return false;
    }

    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string encoded = ss.str();
    if (encoded.empty()) {
        return false;
    }

    return EaSecretObfuscator::decode(encoded, out);
}

bool EaFileSecretStore::saveApiKey(const std::string& key) {
    if (key.empty() || !ensureSecretsDir()) {
        return false;
    }

    std::string encoded;
    if (!EaSecretObfuscator::encode(key, &encoded)) {
        return false;
    }

    const std::string path = secretPath();
    const std::string tmpPath = path + ".tmp";

    FILE* fp = std::fopen(tmpPath.c_str(), "w");
    if (!fp) {
        LOGE("EaFileSecretStore: failed to write %s", tmpPath.c_str());
        return false;
    }

    std::fwrite(encoded.data(), 1, encoded.size(), fp);
    std::fputc('\n', fp);
    std::fflush(fp);
    std::fclose(fp);

    if (::chmod(tmpPath.c_str(), S_IRUSR | S_IWUSR) != 0) {
        LOGE("EaFileSecretStore: chmod failed for %s", tmpPath.c_str());
        return false;
    }

    if (std::rename(tmpPath.c_str(), path.c_str()) != 0) {
        LOGE("EaFileSecretStore: rename failed for %s", path.c_str());
        return false;
    }

    return true;
}

bool EaFileSecretStore::deleteApiKey() {
    if (!hasApiKey()) {
        return true;
    }

    if (std::remove(secretPath().c_str()) != 0) {
        LOGE("EaFileSecretStore: failed to delete %s", secretPath().c_str());
        return false;
    }

    return true;
}

}  // namespace embedagent

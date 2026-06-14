#include <ea_file_storage.h>

#include <ea_session_codec.h>

#include <ev_logger.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

namespace embedagent {

namespace {

bool mkdirRecursive(const std::string& path, mode_t mode) {
    if (path.empty()) {
        return false;
    }

    struct stat st;
    if (::stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }

    const std::size_t slash = path.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
        const std::string parent = path.substr(0, slash);
        if (!mkdirRecursive(parent, mode)) {
            return false;
        }
    }

    if (::mkdir(path.c_str(), mode) == 0) {
        return true;
    }

    if (errno == EEXIST) {
        return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    }

    return false;
}

std::string sanitizeKey(const std::string& key) {
    std::string out;
    out.reserve(key.size());
    for (std::size_t i = 0; i < key.size(); ++i) {
        const char c = key[i];
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_') {
            out.push_back(c);
        } else {
            out.push_back('_');
        }
    }
    return out;
}

}  // namespace

EaFileStorage::EaFileStorage(const std::string& dataDir)
    : dataDir_(dataDir)
{}

bool EaFileStorage::ensureDataDir() const {
    if (dataDir_.empty()) {
        return false;
    }

    if (!mkdirRecursive(dataDir_, S_IRWXU)) {
        LOGE("EaFileStorage: failed to create data dir %s: %s",
             dataDir_.c_str(), std::strerror(errno));
        return false;
    }

    const std::string kvDir = dataDir_ + "/kv";
    if (!mkdirRecursive(kvDir, S_IRWXU)) {
        LOGE("EaFileStorage: failed to create kv dir %s: %s",
             kvDir.c_str(), std::strerror(errno));
        return false;
    }

    return true;
}

bool EaFileStorage::loadSession(EaSession* session) const {
    if (!session) {
        return false;
    }

    const std::string path = dataDir_ + "/session.json";
    std::ifstream in(path.c_str());
    if (!in) {
        return false;
    }

    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string json = ss.str();
    if (json.empty()) {
        return false;
    }

    EaSessionSnapshot snap;
    if (!loadSessionSnapshot(json, &snap)) {
        LOGW("EaFileStorage: failed to parse %s", path.c_str());
        return false;
    }

    return applySessionSnapshot(snap, session);
}

bool EaFileStorage::saveSession(const EaSession& session) {
    if (!ensureDataDir()) {
        return false;
    }

    std::string json;
    if (!serializeSession(session, &json)) {
        return false;
    }

    const std::string path = dataDir_ + "/session.json";
    const std::string tmpPath = path + ".tmp";

    std::ofstream out(tmpPath.c_str(), std::ios::trunc);
    if (!out) {
        LOGE("EaFileStorage: failed to write %s", tmpPath.c_str());
        return false;
    }

    out << json;
    out.flush();
    if (!out.good()) {
        return false;
    }
    out.close();

    if (std::rename(tmpPath.c_str(), path.c_str()) != 0) {
        LOGE("EaFileStorage: rename failed for %s", path.c_str());
        return false;
    }

    return true;
}

std::string EaFileStorage::kvPath(const std::string& key) const {
    return dataDir_ + "/kv/" + sanitizeKey(key) + ".txt";
}

bool EaFileStorage::getString(const std::string& key, std::string* out) const {
    if (!out || key.empty()) {
        return false;
    }

    std::ifstream in(kvPath(key).c_str());
    if (!in) {
        return false;
    }

    std::ostringstream ss;
    ss << in.rdbuf();
    *out = ss.str();
    return true;
}

bool EaFileStorage::putString(const std::string& key, const std::string& value) {
    if (key.empty() || !ensureDataDir()) {
        return false;
    }

    const std::string path = kvPath(key);
    std::ofstream out(path.c_str(), std::ios::trunc);
    if (!out) {
        LOGE("EaFileStorage: failed to write %s", path.c_str());
        return false;
    }

    out << value;
    return out.good();
}

}  // namespace embedagent

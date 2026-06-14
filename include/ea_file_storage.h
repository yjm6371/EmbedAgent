// ea_file_storage.h — minimal file-backed KV storage (Phase 2 subset)
#pragma once

#include <ea_storage.h>

#include <string>

namespace embedagent {

// Ensures dataDir exists with mode 0700.
class EaFileStorage : public EaStorage {
public:
    explicit EaFileStorage(const std::string& dataDir);

    const std::string& dataDir() const { return dataDir_; }

    bool ensureDataDir() const;

    bool loadSession(EaSession* session) const override;
    bool saveSession(const EaSession& session) override;

    bool getString(const std::string& key, std::string* out) const override;
    bool putString(const std::string& key, const std::string& value) override;

private:
    std::string kvPath(const std::string& key) const;

    std::string dataDir_;
};

}  // namespace embedagent

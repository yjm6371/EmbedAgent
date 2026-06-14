#include <gtest/gtest.h>

#include <ea_env_secret_store.h>
#include <ea_file_secret_store.h>
#include <ea_secret_obfuscator.h>
#include <ea_secret_resolver.h>

#include <cstdlib>
#include <ctime>
#include <sys/stat.h>
#include <unistd.h>

using embedagent::EaEnvSecretStore;
using embedagent::EaFileSecretStore;
using embedagent::EaSecretObfuscator;
using embedagent::EaSecretResolver;
using embedagent::EaSecretSource;

namespace {

std::string tempDir(const char* name) {
    return std::string("/tmp/ea_secret_") + name + "_" +
           std::to_string(static_cast<long>(::time(nullptr)));
}

class ScopedEnv {
public:
    ScopedEnv(const char* key, const char* value)
        : key_(key)
        , hadKey_(std::getenv(key) != nullptr)
    {
        if (hadKey_) {
            saved_ = std::getenv(key);
        }
        if (value) {
            ::setenv(key, value, 1);
        } else {
            ::unsetenv(key);
        }
    }

    ~ScopedEnv() {
        if (hadKey_) {
            ::setenv(key_.c_str(), saved_.c_str(), 1);
        } else {
            ::unsetenv(key_.c_str());
        }
    }

private:
    std::string key_;
    bool        hadKey_;
    std::string saved_;
};

}  // namespace

TEST(EaSecretObfuscatorTest, RoundTrip) {
    const std::string plain = "sk-test-key-12345";
    std::string encoded;
    ASSERT_TRUE(EaSecretObfuscator::encode(plain, &encoded));
    EXPECT_NE(encoded, plain);

    std::string decoded;
    ASSERT_TRUE(EaSecretObfuscator::decode(encoded, &decoded));
    EXPECT_EQ(decoded, plain);
}

TEST(EaSecretObfuscatorTest, UnicodeRoundTrip) {
    const std::string plain = "sk-\xE4\xB8\xAD\xE6\x96\x87";
    std::string encoded;
    ASSERT_TRUE(EaSecretObfuscator::encode(plain, &encoded));

    std::string decoded;
    ASSERT_TRUE(EaSecretObfuscator::decode(encoded, &decoded));
    EXPECT_EQ(decoded, plain);
}

TEST(EaEnvSecretStoreTest, LoadFromEnv) {
    ScopedEnv env("EMBEDAGENT_API_KEY", "sk-from-env");

    EaEnvSecretStore store;
    std::string key;
    ASSERT_TRUE(store.loadApiKey(&key));
    EXPECT_EQ(key, "sk-from-env");
    EXPECT_TRUE(store.hasApiKey());
}

TEST(EaFileSecretStoreTest, SaveLoadAndMode) {
    const std::string dir = tempDir("file_secret");
    EaFileSecretStore store(dir);

    ASSERT_TRUE(store.saveApiKey("sk-file-key"));
    EXPECT_TRUE(store.hasApiKey());

    struct stat st;
    ASSERT_EQ(::stat((dir + "/secrets/api_key.enc").c_str(), &st), 0);
    EXPECT_EQ(st.st_mode & 0777, 0600);

    std::string loaded;
    ASSERT_TRUE(store.loadApiKey(&loaded));
    EXPECT_EQ(loaded, "sk-file-key");

    ASSERT_TRUE(store.deleteApiKey());
    EXPECT_FALSE(store.hasApiKey());
}

TEST(EaSecretResolverTest, PriorityCliEnvFile) {
    const std::string dir = tempDir("resolver");
    EaFileSecretStore fileStore(dir);
    ASSERT_TRUE(fileStore.saveApiKey("sk-file"));

    ScopedEnv env("EMBEDAGENT_API_KEY", "sk-env");

    EaSecretResolver resolver(dir);
    resolver.setCliOverride("sk-cli");

    std::string key;
    EaSecretSource source = EaSecretSource::kNone;
    ASSERT_TRUE(resolver.resolveApiKey(&key, &source));
    EXPECT_EQ(key, "sk-cli");
    EXPECT_EQ(source, EaSecretSource::kCli);

    resolver.setCliOverride("");
    ASSERT_TRUE(resolver.resolveApiKey(&key, &source));
    EXPECT_EQ(key, "sk-env");
    EXPECT_EQ(source, EaSecretSource::kEnv);

    ScopedEnv noEnv("EMBEDAGENT_API_KEY", nullptr);
    ASSERT_TRUE(resolver.resolveApiKey(&key, &source));
    EXPECT_EQ(key, "sk-file");
    EXPECT_EQ(source, EaSecretSource::kFile);
}

TEST(EaSecretResolverTest, SaveApiKeyToFile) {
    const std::string dir = tempDir("save");
    EaSecretResolver resolver(dir);

    ASSERT_TRUE(resolver.saveApiKeyToFile("sk-saved"));

    resolver.setCliOverride("");
    ScopedEnv env("EMBEDAGENT_API_KEY", nullptr);

    std::string key;
    EaSecretSource source = EaSecretSource::kNone;
    ASSERT_TRUE(resolver.resolveApiKey(&key, &source));
    EXPECT_EQ(key, "sk-saved");
    EXPECT_EQ(source, EaSecretSource::kFile);
}

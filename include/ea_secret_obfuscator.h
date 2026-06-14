// ea_secret_obfuscator.h — XOR + Base64 obfuscation (not strong encryption)
#pragma once

#include <string>

namespace embedagent {

class EaSecretObfuscator {
public:
    static bool encode(const std::string& plain, std::string* out);
    static bool decode(const std::string& encoded, std::string* out);

    static std::string pepper();

private:
    static void xorInPlace(std::string* data, const std::string& pepper);
    static bool base64Encode(const std::string& input, std::string* out);
    static bool base64Decode(const std::string& input, std::string* out);
};

}  // namespace embedagent

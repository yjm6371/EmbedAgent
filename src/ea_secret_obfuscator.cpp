#include <ea_secret_obfuscator.h>

#include <cctype>
#include <fstream>
#include <vector>

namespace embedagent {

namespace {

const char kEmbedAgentPepper[] = "cppev-embedagent-v1";

std::string readMachineIdPrefix() {
    std::ifstream in("/etc/machine-id");
    if (!in) {
        return std::string();
    }

    std::string line;
    std::getline(in, line);
    if (line.size() > 16) {
        line.resize(16);
    }
    return line;
}

const char* base64Chars() {
    return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}

}  // namespace

std::string EaSecretObfuscator::pepper() {
    return std::string(kEmbedAgentPepper) + readMachineIdPrefix();
}

void EaSecretObfuscator::xorInPlace(std::string* data,
                                    const std::string& pepper) {
    if (!data || pepper.empty()) {
        return;
    }

    for (std::size_t i = 0; i < data->size(); ++i) {
        (*data)[i] = static_cast<char>(
            (*data)[i] ^ pepper[i % pepper.size()]);
    }
}

bool EaSecretObfuscator::base64Encode(const std::string& input,
                                      std::string* out) {
    if (!out) {
        return false;
    }

    out->clear();
    const unsigned char* bytes =
        reinterpret_cast<const unsigned char*>(input.data());
    const std::size_t len = input.size();

    for (std::size_t i = 0; i < len; i += 3) {
        const unsigned int b0 = bytes[i];
        const unsigned int b1 = (i + 1 < len) ? bytes[i + 1] : 0;
        const unsigned int b2 = (i + 2 < len) ? bytes[i + 2] : 0;
        const unsigned int triple = (b0 << 16) | (b1 << 8) | b2;

        out->push_back(base64Chars()[(triple >> 18) & 0x3F]);
        out->push_back(base64Chars()[(triple >> 12) & 0x3F]);
        out->push_back((i + 1 < len) ? base64Chars()[(triple >> 6) & 0x3F] : '=');
        out->push_back((i + 2 < len) ? base64Chars()[triple & 0x3F] : '=');
    }

    return true;
}

bool EaSecretObfuscator::base64Decode(const std::string& input,
                                      std::string* out) {
    if (!out || input.empty()) {
        return false;
    }

    auto decodeChar = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') {
            return c - 'A';
        }
        if (c >= 'a' && c <= 'z') {
            return c - 'a' + 26;
        }
        if (c >= '0' && c <= '9') {
            return c - '0' + 52;
        }
        if (c == '+') {
            return 62;
        }
        if (c == '/') {
            return 63;
        }
        return -1;
    };

    out->clear();
    std::vector<unsigned char> bytes;
    bytes.reserve(input.size());

    int val = 0;
    int valb = -8;
    for (std::size_t i = 0; i < input.size(); ++i) {
        const char c = input[i];
        if (c == '=') {
            break;
        }
        if (std::isspace(static_cast<unsigned char>(c))) {
            continue;
        }

        const int d = decodeChar(c);
        if (d < 0) {
            return false;
        }

        val = (val << 6) | d;
        valb += 6;
        if (valb >= 0) {
            bytes.push_back(static_cast<unsigned char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }

    out->assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return true;
}

bool EaSecretObfuscator::encode(const std::string& plain, std::string* out) {
    if (!out) {
        return false;
    }

    std::string buf = plain;
    xorInPlace(&buf, pepper());
    return base64Encode(buf, out);
}

bool EaSecretObfuscator::decode(const std::string& encoded, std::string* out) {
    if (!out || encoded.empty()) {
        return false;
    }

    std::string buf;
    if (!base64Decode(encoded, &buf)) {
        return false;
    }

    xorInPlace(&buf, pepper());
    *out = buf;
    return true;
}

}  // namespace embedagent

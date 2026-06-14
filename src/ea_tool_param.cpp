#include <ea_tool_param.h>

namespace embedagent {

namespace {

const char* paramTypeString(EaParamType t) {
    switch (t) {
        case EaParamType::kString:  return "string";
        case EaParamType::kInteger: return "integer";
        case EaParamType::kNumber:  return "number";
        case EaParamType::kBoolean: return "boolean";
    }
    return "string";
}

// Minimal JSON string escaping for human-supplied field names and descriptions.
std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if      (c == '"')  { out += "\\\""; }
        else if (c == '\\') { out += "\\\\"; }
        else if (c == '\n') { out += "\\n";  }
        else if (c == '\r') { out += "\\r";  }
        else if (c == '\t') { out += "\\t";  }
        else                { out += c;      }
    }
    return out;
}

}  // namespace

bool buildParametersJson(const std::vector<EaToolParam>& params,
                         std::string* out) {
    if (!out) {
        return false;
    }

    std::string result;
    result.reserve(128);
    result += "{\"type\":\"object\",\"properties\":{";

    for (std::size_t i = 0; i < params.size(); ++i) {
        const EaToolParam& p = params[i];
        if (i > 0) {
            result += ',';
        }

        result += '"';
        result += jsonEscape(p.name);
        result += "\":{\"type\":\"";
        result += paramTypeString(p.type);
        result += '"';

        if (!p.description.empty()) {
            result += ",\"description\":\"";
            result += jsonEscape(p.description);
            result += '"';
        }

        if (!p.enumValues.empty()) {
            result += ",\"enum\":[";
            for (std::size_t j = 0; j < p.enumValues.size(); ++j) {
                if (j > 0) {
                    result += ',';
                }
                // Numeric types: emit bare numeric literal; strings: quoted.
                if (p.type == EaParamType::kInteger ||
                    p.type == EaParamType::kNumber) {
                    result += p.enumValues[j];
                } else {
                    result += '"';
                    result += jsonEscape(p.enumValues[j]);
                    result += '"';
                }
            }
            result += ']';
        }

        result += '}';
    }

    result += '}';

    // Collect required parameter names.
    bool firstReq = true;
    for (std::size_t i = 0; i < params.size(); ++i) {
        if (!params[i].required) {
            continue;
        }
        if (firstReq) {
            result += ",\"required\":[";
            firstReq = false;
        } else {
            result += ',';
        }
        result += '"';
        result += jsonEscape(params[i].name);
        result += '"';
    }
    if (!firstReq) {
        result += ']';
    }

    result += '}';
    *out = result;
    return true;
}

}  // namespace embedagent

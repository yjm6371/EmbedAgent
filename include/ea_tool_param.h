// ea_tool_param.h — structured tool parameter descriptors and JSON Schema builder
#pragma once

#include <string>
#include <vector>

namespace embedagent {

// JSON Schema primitive types for tool parameters.
enum class EaParamType {
    kString,
    kInteger,
    kNumber,
    kBoolean,
};

// Describes a single named parameter accepted by an LLM tool.
struct EaToolParam {
    std::string              name;
    EaParamType              type        {EaParamType::kString};
    std::string              description;
    bool                     required    {true};
    // Optional allowed values; for kInteger/kNumber, strings must be numeric literals.
    std::vector<std::string> enumValues;
};

// Generates the JSON Schema "parameters" object fragment expected by OpenAI
// tool definitions.  On success writes to *out and returns true.
bool buildParametersJson(const std::vector<EaToolParam>& params,
                         std::string* out);

}  // namespace embedagent

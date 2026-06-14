// ea_tool_builder.h — fluent builder for EaToolSpec with structured parameters
//
// EaToolBuilder converts a declarative parameter list into the raw JSON Schema
// string required by EaToolSpec::def.parametersJson, and adapts the typed
// EaToolArgsHandler into the lower-level EaToolHandler expected by
// EaToolRegistry.  It is a header-only helper; no separate .cpp is needed.
#pragma once

#include <ea_tool_args.h>
#include <ea_tool_param.h>
#include <ea_tool_registry.h>

#include <string>
#include <vector>

namespace embedagent {

class EaToolBuilder {
public:
    EaToolBuilder() {}

    EaToolBuilder& name(const std::string& n) {
        name_ = n;
        return *this;
    }

    EaToolBuilder& description(const std::string& d) {
        description_ = d;
        return *this;
    }

    // Append a plain parameter (no restricted value set).
    EaToolBuilder& param(const std::string& paramName,
                         EaParamType        type,
                         const std::string& desc,
                         bool               required = true) {
        EaToolParam p;
        p.name        = paramName;
        p.type        = type;
        p.description = desc;
        p.required    = required;
        params_.push_back(p);
        return *this;
    }

    // Append a parameter with an explicit set of allowed values (JSON Schema enum).
    // For kInteger / kNumber params, values must be bare numeric strings, e.g. "0".
    EaToolBuilder& enumParam(const std::string&              paramName,
                             EaParamType                     type,
                             const std::string&              desc,
                             const std::vector<std::string>& values,
                             bool                            required = true) {
        EaToolParam p;
        p.name        = paramName;
        p.type        = type;
        p.description = desc;
        p.required    = required;
        p.enumValues  = values;
        params_.push_back(p);
        return *this;
    }

    // Set the typed handler that receives parsed arguments.
    EaToolBuilder& handler(EaToolArgsHandler h) {
        handler_ = h;
        return *this;
    }

    // Build an EaToolSpec ready for EaToolRegistry::registerTool().
    // Returns a spec with an empty name on failure (missing name or handler).
    EaToolSpec build() const {
        EaToolSpec spec;
        spec.def.name        = name_;
        spec.def.description = description_;

        std::string paramsJson;
        if (!buildParametersJson(params_, &paramsJson)) {
            paramsJson = "{\"type\":\"object\",\"properties\":{}}";
        }
        spec.def.parametersJson = paramsJson;

        if (handler_) {
            // Copy handler by value so the lambda outlives this builder.
            EaToolArgsHandler h = handler_;
            spec.handler = [h](const std::string& argsJson,
                               std::string*       resultJson) {
                EaToolArgs args(argsJson);
                return h(args, resultJson);
            };
        }

        return spec;
    }

private:
    std::string              name_;
    std::string              description_;
    std::vector<EaToolParam> params_;
    EaToolArgsHandler        handler_;
};

}  // namespace embedagent

// ea_tool_registry.h — local tool registration and invocation
#pragma once

#include <ea_tool_args.h>
#include <ea_tool_param.h>
#include <ev_llm_json.h>

#include <functional>
#include <string>
#include <vector>

namespace embedagent {

// Low-level handler: receives the raw arguments JSON string from the model.
using EaToolHandler =
    std::function<bool(const std::string& argumentsJson, std::string* resultJson)>;

// High-level handler: receives a pre-parsed, type-safe argument accessor.
using EaToolArgsHandler =
    std::function<bool(const EaToolArgs& args, std::string* resultJson)>;

struct EaToolSpec {
    cppev::ext::EvLlmToolDef def;
    EaToolHandler            handler;
};

class EaToolRegistry {
public:
    // Register a tool from a fully-constructed spec.
    // Returns false if the name is empty, handler is null, or name is already taken.
    bool registerTool(const EaToolSpec& spec);

    // Convenience overload: builds the JSON Schema from a structured param list
    // and wraps the typed handler automatically.
    // Returns false on name collision or invalid arguments.
    bool registerTool(const std::string&              name,
                      const std::string&              description,
                      const std::vector<EaToolParam>& params,
                      EaToolArgsHandler               handler);

    bool invoke(const std::string& name,
                const std::string& argumentsJson,
                std::string*       resultJson) const;

    std::vector<cppev::ext::EvLlmToolDef> toolDefinitions() const;
    bool empty() const { return tools_.empty(); }
    std::size_t size() const { return tools_.size(); }

private:
    std::vector<EaToolSpec> tools_;
};

}  // namespace embedagent

#include <ea_tool_registry.h>

#include <ev_logger.h>

namespace embedagent {

bool EaToolRegistry::registerTool(const EaToolSpec& spec) {
    if (spec.def.name.empty() || !spec.handler) {
        LOGW("EaToolRegistry: registerTool rejected — empty name or null handler");
        return false;
    }

    for (std::size_t i = 0; i < tools_.size(); ++i) {
        if (tools_[i].def.name == spec.def.name) {
            LOGW("EaToolRegistry: tool \"%s\" is already registered (duplicate ignored)",
                 spec.def.name.c_str());
            return false;
        }
    }

    tools_.push_back(spec);
    return true;
}

bool EaToolRegistry::registerTool(const std::string&              name,
                                   const std::string&              description,
                                   const std::vector<EaToolParam>& params,
                                   EaToolArgsHandler               handler) {
    if (name.empty() || !handler) {
        LOGW("EaToolRegistry: registerTool rejected — empty name or null handler");
        return false;
    }

    std::string paramsJson;
    if (!buildParametersJson(params, &paramsJson)) {
        LOGW("EaToolRegistry: buildParametersJson failed for \"%s\"", name.c_str());
        return false;
    }

    // Wrap typed handler; EaToolArgs is constructed on each invocation.
    EaToolSpec spec;
    spec.def.name           = name;
    spec.def.description    = description;
    spec.def.parametersJson = paramsJson;
    spec.handler = [handler](const std::string& argsJson,
                             std::string*       resultJson) {
        EaToolArgs args(argsJson);
        return handler(args, resultJson);
    };

    return registerTool(spec);
}

bool EaToolRegistry::invoke(const std::string& name,
                            const std::string& argumentsJson,
                            std::string* resultJson) const {
    if (!resultJson) {
        return false;
    }

    for (std::size_t i = 0; i < tools_.size(); ++i) {
        if (tools_[i].def.name == name) {
            return tools_[i].handler(argumentsJson, resultJson);
        }
    }

    // Build a comma-separated list of registered names for the error message.
    std::string available;
    for (std::size_t i = 0; i < tools_.size(); ++i) {
        if (i > 0) { available += ", "; }
        available += tools_[i].def.name;
    }
    LOGE("EaToolRegistry: unknown tool \"%s\" (registered: %s)",
         name.c_str(), available.empty() ? "(none)" : available.c_str());
    return false;
}

std::vector<cppev::ext::EvLlmToolDef> EaToolRegistry::toolDefinitions() const {
    std::vector<cppev::ext::EvLlmToolDef> defs;
    defs.reserve(tools_.size());
    for (std::size_t i = 0; i < tools_.size(); ++i) {
        defs.push_back(tools_[i].def);
    }
    return defs;
}

}  // namespace embedagent

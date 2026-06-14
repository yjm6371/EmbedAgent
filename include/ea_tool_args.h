// ea_tool_args.h — typed accessor wrapper for LLM tool-call argument JSON
#pragma once

#include <memory>
#include <string>

namespace cppev {
namespace ext {
class EvJsonDocument;
}  // namespace ext
}  // namespace cppev

namespace embedagent {

// Wraps the raw argumentsJson string emitted by the model and exposes
// type-safe getters.  Construct once per tool invocation.
//
// Note: getDouble() falls back to integer precision because the underlying
// JSON library exposes numbers as int.  Use kNumber params for documentation
// purposes; fractional values may be truncated when read back.
class EaToolArgs {
public:
    explicit EaToolArgs(const std::string& argsJson);
    ~EaToolArgs();

    EaToolArgs(const EaToolArgs&)            = delete;
    EaToolArgs& operator=(const EaToolArgs&) = delete;

    // Returns true if argsJson was parsed successfully.
    bool valid() const { return valid_; }

    std::string getString(const std::string& key,
                          const std::string& def = "") const;
    int         getInt   (const std::string& key, int    def = 0)     const;
    double      getDouble(const std::string& key, double def = 0.0)   const;
    bool        getBool  (const std::string& key, bool   def = false) const;

    // Returns true if the key is present with any primitive value.
    bool has(const std::string& key) const;

private:
    bool                                        valid_;
    std::unique_ptr<cppev::ext::EvJsonDocument> doc_;
};

}  // namespace embedagent

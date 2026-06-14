// ea_prompt_template.h — {{variable}} substitution for system prompts
#pragma once

#include <map>
#include <string>

namespace embedagent {

class EaPromptTemplate {
public:
    void setTemplate(const std::string& tmpl) { template_ = tmpl; }
    const std::string& templateText() const { return template_; }

    void setVariable(const std::string& key, const std::string& value);
    void clearVariables();

    // Single-pass {{key}} replacement; unknown keys are left unchanged.
    std::string render() const;

private:
    std::string              template_;
    std::map<std::string, std::string> variables_;
};

}  // namespace embedagent

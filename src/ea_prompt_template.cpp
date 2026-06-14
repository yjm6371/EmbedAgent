#include <ea_prompt_template.h>

namespace embedagent {

void EaPromptTemplate::setVariable(const std::string& key,
                                   const std::string& value) {
    variables_[key] = value;
}

void EaPromptTemplate::clearVariables() {
    variables_.clear();
}

std::string EaPromptTemplate::render() const {
    std::string result = template_;

    for (std::map<std::string, std::string>::const_iterator it =
             variables_.begin();
         it != variables_.end(); ++it) {
        const std::string placeholder = "{{" + it->first + "}}";
        std::string::size_type pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.size(), it->second);
            pos += it->second.size();
        }
    }

    return result;
}

}  // namespace embedagent

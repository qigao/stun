#pragma once

#include "../ir/unified_diagram.h"
#include <string>
#include <unordered_map>

namespace flex::modules::flexmaid {

class TemplateManager {
public:
    static TemplateManager& instance();
    
    const std::string& get_template(DiagramType type) const;
    const std::string& get_partial(const std::string& name) const;
    
    void set_template_dir(const std::string& dir);
    void reload();
    
private:
    TemplateManager();
    void load_embedded_templates();
    
    std::unordered_map<DiagramType, std::string> templates_;
    std::unordered_map<std::string, std::string> partials_;
    std::string template_dir_;
};

} // namespace flex::modules::flexmaid

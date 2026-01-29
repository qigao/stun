/*
 * flexUI Designer - Template-based Code Generator
 *
 * Uses mustache templates for flexible code generation.
 */

#pragma once

#include "designer.h"
#include <string>
#include <vector>
#include <map>

namespace flexui_designer {

class TemplateGenerator {
public:
    TemplateGenerator();
    ~TemplateGenerator();

    // Generate code using template
    std::string generate(const std::vector<DesignWidget>& widgets, const std::string& template_name = "cpp") const;
    
    // Save generated code
    bool save(const std::string& path, const std::string& code) const;

    // Available templates
    static const char* get_cpp_template();
    static const char* get_json_template();
    static const char* get_xml_template();
    
    static std::string widget_type_name(WidgetType type);
    static std::string escape_string(const std::string& s);

private:
    std::string render_template(const std::string& tmpl, const std::vector<DesignWidget>& widgets) const;
};

} // namespace flexui_designer

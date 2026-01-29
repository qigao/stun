/*
 * flexUI Designer - Code Generator
 *
 * Generates C++ code from the design.
 */

#pragma once

#include "designer.h"
#include <string>
#include <sstream>

namespace flexui_designer {

class CodeGenerator {
public:
    std::string generate(const std::vector<DesignWidget>& widgets) const;
    bool save(const std::string& path, const std::string& code) const;

private:
    std::string widget_type_name(WidgetType type) const;
    std::string generate_widget_code(const DesignWidget& w, int indent) const;
    std::string indent_str(int level) const;
};

} // namespace flexui_designer

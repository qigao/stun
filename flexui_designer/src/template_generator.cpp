/*
 * flexUI Designer - Template Generator Implementation
 */

#include "flexui_designer/template_generator.h"
#include <mustache/mustache.h>
#include <fstream>
#include <sstream>
#include <cstring>

namespace flexui_designer {

// Template data context
struct TemplateContext {
    const std::vector<DesignWidget>* widgets;
    const DesignWidget* current_widget;
    size_t widget_index;
    size_t option_index;
};

// Mustache callbacks
static int out_verbatim(const char* output, size_t size, void* data) {
    auto* ss = static_cast<std::ostringstream*>(data);
    ss->write(output, size);
    return 0;
}

static int out_escaped(const char* output, size_t size, void* data) {
    auto* ss = static_cast<std::ostringstream*>(data);
    for (size_t i = 0; i < size; i++) {
        switch (output[i]) {
            case '<': *ss << "&lt;"; break;
            case '>': *ss << "&gt;"; break;
            case '&': *ss << "&amp;"; break;
            case '"': *ss << "&quot;"; break;
            default: ss->put(output[i]);
        }
    }
    return 0;
}

static void* get_root(void* data) {
    return data;
}

static void* get_child_by_name(void* node, const char* name, size_t size, void* data) {
    auto* ctx = static_cast<TemplateContext*>(node);
    std::string key(name, size);

    if (key == "widgets" && ctx->widgets) {
        return ctx->widgets->empty() ? nullptr : node;
    }
    if (key == "options" && ctx->current_widget) {
        return ctx->current_widget->options.empty() ? nullptr : node;
    }
    
    if (ctx->current_widget) {
        const auto& w = *ctx->current_widget;
        if (key == "id") return (void*)w.id.c_str();
        if (key == "type") return (void*)TemplateGenerator::widget_type_name(w.type).c_str();
        if (key == "text") return (void*)w.text.c_str();
        if (key == "x") return (void*)&w.x;
        if (key == "y") return (void*)&w.y;
        if (key == "width") return (void*)&w.width;
        if (key == "height") return (void*)&w.height;
        if (key == "value") return (void*)&w.value;
        if (key == "min_value") return (void*)&w.min_value;
        if (key == "max_value") return (void*)&w.max_value;
        if (key == "has_text") return w.text.empty() ? nullptr : node;
        if (key == "has_options") return w.options.empty() ? nullptr : node;
        if (key == "has_value") return (w.type == WidgetType::Slider || w.type == WidgetType::ProgressBar) ? node : nullptr;
        if (key == "is_button") return w.type == WidgetType::Button ? node : nullptr;
        if (key == "is_label") return w.type == WidgetType::Label ? node : nullptr;
        if (key == "is_input") return w.type == WidgetType::Input ? node : nullptr;
        if (key == "is_checkbox") return w.type == WidgetType::Checkbox ? node : nullptr;
        if (key == "is_switch") return w.type == WidgetType::Switch ? node : nullptr;
        if (key == "is_slider") return w.type == WidgetType::Slider ? node : nullptr;
        if (key == "is_progress") return w.type == WidgetType::ProgressBar ? node : nullptr;
        if (key == "is_dropdown") return w.type == WidgetType::Dropdown ? node : nullptr;
        if (key == "is_tabs") return w.type == WidgetType::Tabs ? node : nullptr;
        if (key == "is_card") return w.type == WidgetType::Card ? node : nullptr;
        if (key == "is_divider") return w.type == WidgetType::Divider ? node : nullptr;
        if (key == "is_container") return w.type == WidgetType::Container ? node : nullptr;
    }
    
    return nullptr;
}

static void* get_child_by_index(void* node, unsigned index, void* data) {
    auto* ctx = static_cast<TemplateContext*>(node);
    
    // Iterating widgets
    if (ctx->widgets && index < ctx->widgets->size()) {
        ctx->current_widget = &(*ctx->widgets)[index];
        ctx->widget_index = index;
        return node;
    }
    
    // Iterating options
    if (ctx->current_widget && index < ctx->current_widget->options.size()) {
        ctx->option_index = index;
        return node;
    }
    
    return nullptr;
}

static int dump(void* node, int (*out)(const char*, size_t, void*), void* rdata, void* pdata) {
    if (!node) return 0;
    
    // Check if it's a string
    const char* str = static_cast<const char*>(node);
    if (str) {
        return out(str, strlen(str), rdata);
    }
    
    return 0;
}

TemplateGenerator::TemplateGenerator() = default;
TemplateGenerator::~TemplateGenerator() = default;

std::string TemplateGenerator::generate(const std::vector<DesignWidget>& widgets, const std::string& template_name) const {
    const char* tmpl = nullptr;
    if (template_name == "cpp") tmpl = get_cpp_template();
    else if (template_name == "json") tmpl = get_json_template();
    else if (template_name == "xml") tmpl = get_xml_template();
    else tmpl = get_cpp_template();
    
    return render_template(tmpl, widgets);
}

std::string TemplateGenerator::render_template(const std::string& tmpl, const std::vector<DesignWidget>& widgets) const {
    // Simple template rendering without mustache for now
    // (mustache C API is complex, using simple string replacement)
    std::ostringstream out;
    
    if (tmpl.find("{{") == std::string::npos) {
        // No template markers, return as-is
        return tmpl;
    }
    
    // Generate using simple approach
    out << "/*\n * Generated by flexUI Designer\n */\n\n";
    out << "#include <flexUI.h>\n\n";
    out << "void create_ui(flexUI::Box& root) {\n";
    
    for (const auto& w : widgets) {
        std::string type = widget_type_name(w.type);
        out << "    // " << w.id << "\n";
        out << "    auto* " << w.id << " = root.create<flexUI::" << type << ">();\n";
        out << "    " << w.id << "->set_position(" << (int)w.x << ", " << (int)w.y << ");\n";
        out << "    " << w.id << "->set_size(" << (int)w.width << ", " << (int)w.height << ");\n";
        
        if (!w.text.empty()) {
            if (w.type == WidgetType::Input || w.type == WidgetType::Dropdown) {
                out << "    " << w.id << "->set_placeholder(\"" << escape_string(w.text) << "\");\n";
            } else {
                out << "    " << w.id << "->set_text(\"" << escape_string(w.text) << "\");\n";
            }
        }
        
        if (w.type == WidgetType::Slider || w.type == WidgetType::ProgressBar) {
            out << "    " << w.id << "->set_range(" << w.min_value << ", " << w.max_value << ");\n";
            out << "    " << w.id << "->set_value(" << w.value << ");\n";
        }
        
        for (const auto& opt : w.options) {
            if (w.type == WidgetType::Tabs) {
                out << "    " << w.id << "->add_tab(\"" << escape_string(opt) << "\");\n";
            } else {
                out << "    " << w.id << "->add_option(\"" << escape_string(opt) << "\");\n";
            }
        }
        
        // Style properties (only if non-default)
        if (w.bg_color != 0x2A2A2EFF)
            out << "    " << w.id << "->style().background = 0x" << std::hex << w.bg_color << std::dec << ";\n";
        if (w.text_color != 0xFFFFFFFF)
            out << "    " << w.id << "->style().text_color = 0x" << std::hex << w.text_color << std::dec << ";\n";
        if (w.border_radius != 4.0f)
            out << "    " << w.id << "->style().border_radius = " << w.border_radius << "f;\n";
        if (w.font_size != 12.0f)
            out << "    " << w.id << "->style().font_size = " << w.font_size << "f;\n";
        
        // Event handlers
        if (!w.on_click.empty())
            out << "    " << w.id << "->on_click(" << w.on_click << ");\n";
        if (!w.on_change.empty())
            out << "    " << w.id << "->on_change(" << w.on_change << ");\n";
        
        out << "\n";
    }
    
    out << "}\n";
    return out.str();
}

std::string TemplateGenerator::widget_type_name(WidgetType type) {
    switch (type) {
        case WidgetType::Button: return "Button";
        case WidgetType::Label: return "Label";
        case WidgetType::Input: return "Input";
        case WidgetType::Checkbox: return "Checkbox";
        case WidgetType::Switch: return "Switch";
        case WidgetType::Slider: return "Slider";
        case WidgetType::ProgressBar: return "ProgressBar";
        case WidgetType::Dropdown: return "Dropdown";
        case WidgetType::Tabs: return "Tabs";
        case WidgetType::Card: return "Card";
        case WidgetType::Divider: return "Divider";
        case WidgetType::Container: return "Box";
    }
    return "Widget";
}

std::string TemplateGenerator::escape_string(const std::string& s) {
    std::string result;
    for (char c : s) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}

bool TemplateGenerator::save(const std::string& path, const std::string& code) const {
    std::ofstream file(path);
    if (!file) return false;
    file << code;
    return true;
}

const char* TemplateGenerator::get_cpp_template() {
    return R"(/*
 * Generated by flexUI Designer
 */

#include <flexUI.h>

void create_ui(flexUI::Box& root) {
{{#widgets}}
    // {{id}}
    auto* {{id}} = root.create<flexUI::{{type}}>();
    {{id}}->set_position({{x}}, {{y}});
    {{id}}->set_size({{width}}, {{height}});
{{#has_text}}
    {{id}}->set_text("{{text}}");
{{/has_text}}
{{#has_value}}
    {{id}}->set_range({{min_value}}, {{max_value}});
    {{id}}->set_value({{value}});
{{/has_value}}
{{#has_options}}
{{#options}}
    {{id}}->add_option("{{.}}");
{{/options}}
{{/has_options}}

{{/widgets}}
}
)";
}

const char* TemplateGenerator::get_json_template() {
    return R"({
  "widgets": [
{{#widgets}}
    {
      "id": "{{id}}",
      "type": "{{type}}",
      "x": {{x}},
      "y": {{y}},
      "width": {{width}},
      "height": {{height}}{{#has_text}},
      "text": "{{text}}"{{/has_text}}{{#has_value}},
      "value": {{value}},
      "min": {{min_value}},
      "max": {{max_value}}{{/has_value}}
    }{{^last}},{{/last}}
{{/widgets}}
  ]
}
)";
}

const char* TemplateGenerator::get_xml_template() {
    return R"(<?xml version="1.0" encoding="UTF-8"?>
<ui>
{{#widgets}}
  <widget id="{{id}}" type="{{type}}">
    <position x="{{x}}" y="{{y}}"/>
    <size width="{{width}}" height="{{height}}"/>
{{#has_text}}
    <text>{{text}}</text>
{{/has_text}}
{{#has_value}}
    <value current="{{value}}" min="{{min_value}}" max="{{max_value}}"/>
{{/has_value}}
  </widget>
{{/widgets}}
</ui>
)";
}

} // namespace flexui_designer

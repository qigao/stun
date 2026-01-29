/*
 * flexUI Designer - Design Unit Implementation
 */

#include "flexui_designer/design_unit.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace flexui_designer {

DesignUnit* UnitManager::create_unit(const std::string& name) {
    DesignUnit unit;
    unit.name = name;
    unit.layout_path = name + ".fuid";
    unit.header_path = name + ".h";
    unit.source_path = name + ".cpp";
    project_.units.push_back(unit);
    return &project_.units.back();
}

DesignUnit* UnitManager::get_unit(const std::string& name) {
    for (auto& u : project_.units) {
        if (u.name == name) return &u;
    }
    return nullptr;
}

bool UnitManager::delete_unit(const std::string& name) {
    auto it = std::find_if(project_.units.begin(), project_.units.end(),
        [&](const DesignUnit& u) { return u.name == name; });
    if (it != project_.units.end()) {
        project_.units.erase(it);
        return true;
    }
    return false;
}

bool UnitManager::rename_unit(const std::string& old_name, const std::string& new_name) {
    auto* unit = get_unit(old_name);
    if (!unit) return false;
    unit->name = new_name;
    unit->layout_path = new_name + ".fuid";
    unit->header_path = new_name + ".h";
    unit->source_path = new_name + ".cpp";
    return true;
}

std::string UnitManager::create_group(DesignUnit& unit, const std::vector<std::string>& widget_ids, const std::string& name) {
    WidgetGroup group;
    group.id = "group_" + std::to_string(next_group_id_++);
    group.name = name.empty() ? group.id : name;
    group.widget_ids = widget_ids;
    
    // Update widgets' group_id
    for (auto& w : unit.widgets) {
        if (std::find(widget_ids.begin(), widget_ids.end(), w.id) != widget_ids.end()) {
            w.group_id = group.id;
        }
    }
    
    unit.groups.push_back(group);
    return group.id;
}

bool UnitManager::ungroup(DesignUnit& unit, const std::string& group_id) {
    auto it = std::find_if(unit.groups.begin(), unit.groups.end(),
        [&](const WidgetGroup& g) { return g.id == group_id; });
    if (it == unit.groups.end()) return false;
    
    // Clear group_id from widgets
    for (auto& w : unit.widgets) {
        if (w.group_id == group_id) w.group_id.clear();
    }
    
    unit.groups.erase(it);
    return true;
}

WidgetGroup* UnitManager::get_group(DesignUnit& unit, const std::string& group_id) {
    for (auto& g : unit.groups) {
        if (g.id == group_id) return &g;
    }
    return nullptr;
}

std::string UnitManager::generate_header(const DesignUnit& unit) {
    std::ostringstream out;
    std::string guard = unit.name;
    std::transform(guard.begin(), guard.end(), guard.begin(), ::toupper);
    
    out << "#pragma once\n\n";
    out << "#include <flexUI.h>\n\n";
    
    if (!project_.namespace_name.empty()) {
        out << "namespace " << project_.namespace_name << " {\n\n";
    }
    
    out << "class " << unit.name << " {\n";
    out << "public:\n";
    out << "    " << unit.name << "();\n";
    out << "    ~" << unit.name << "() = default;\n\n";
    out << "    void create(flexUI::Box& parent);\n\n";
    out << "private:\n";
    
    // Widget members
    for (const auto& w : unit.widgets) {
        std::string type;
        switch (w.type) {
            case WidgetType::Button: type = "Button"; break;
            case WidgetType::Label: type = "Label"; break;
            case WidgetType::Input: type = "Input"; break;
            case WidgetType::Checkbox: type = "Checkbox"; break;
            case WidgetType::Switch: type = "Switch"; break;
            case WidgetType::Slider: type = "Slider"; break;
            case WidgetType::ProgressBar: type = "ProgressBar"; break;
            case WidgetType::Dropdown: type = "Dropdown"; break;
            case WidgetType::Tabs: type = "Tabs"; break;
            case WidgetType::Card: type = "Card"; break;
            case WidgetType::Divider: type = "Divider"; break;
            case WidgetType::Container: type = "Box"; break;
        }
        out << "    flexUI::" << type << "* " << w.id << "_ = nullptr;\n";
    }
    
    // Event handlers
    out << "\n    // Event handlers\n";
    for (const auto& h : unit.handlers) {
        out << "    void " << h.handler_name << "();\n";
    }
    
    out << "};\n";
    
    if (!project_.namespace_name.empty()) {
        out << "\n} // namespace " << project_.namespace_name << "\n";
    }
    
    return out.str();
}

std::string UnitManager::generate_source(const DesignUnit& unit) {
    std::ostringstream out;
    
    out << "#include \"" << unit.header_path << "\"\n\n";
    
    if (!project_.namespace_name.empty()) {
        out << "namespace " << project_.namespace_name << " {\n\n";
    }
    
    out << unit.name << "::" << unit.name << "() {}\n\n";
    
    out << "void " << unit.name << "::create(flexUI::Box& parent) {\n";
    
    for (const auto& w : unit.widgets) {
        std::string type;
        switch (w.type) {
            case WidgetType::Button: type = "Button"; break;
            case WidgetType::Label: type = "Label"; break;
            case WidgetType::Input: type = "Input"; break;
            case WidgetType::Checkbox: type = "Checkbox"; break;
            case WidgetType::Switch: type = "Switch"; break;
            case WidgetType::Slider: type = "Slider"; break;
            case WidgetType::ProgressBar: type = "ProgressBar"; break;
            case WidgetType::Dropdown: type = "Dropdown"; break;
            case WidgetType::Tabs: type = "Tabs"; break;
            case WidgetType::Card: type = "Card"; break;
            case WidgetType::Divider: type = "Divider"; break;
            case WidgetType::Container: type = "Box"; break;
        }
        
        out << "    " << w.id << "_ = parent.create<flexUI::" << type << ">();\n";
        out << "    " << w.id << "_->set_position(" << (int)w.x << ", " << (int)w.y << ");\n";
        out << "    " << w.id << "_->set_size(" << (int)w.width << ", " << (int)w.height << ");\n";
        if (!w.text.empty()) {
            out << "    " << w.id << "_->set_text(\"" << w.text << "\");\n";
        }
        out << "\n";
    }
    
    out << "}\n";
    
    // Event handler stubs
    for (const auto& h : unit.handlers) {
        out << "\nvoid " << unit.name << "::" << h.handler_name << "() {\n";
        out << "    // TODO: Implement\n";
        out << "}\n";
    }
    
    if (!project_.namespace_name.empty()) {
        out << "\n} // namespace " << project_.namespace_name << "\n";
    }
    
    return out.str();
}

bool UnitManager::save_project(const DesignProject& project, const std::string& path) {
    std::ofstream file(path);
    if (!file) return false;
    
    file << "{\n";
    file << "  \"name\": \"" << project.name << "\",\n";
    file << "  \"main_unit\": \"" << project.main_unit << "\",\n";
    file << "  \"namespace\": \"" << project.namespace_name << "\",\n";
    file << "  \"units\": [\n";
    
    for (size_t i = 0; i < project.units.size(); ++i) {
        const auto& u = project.units[i];
        file << "    {\"name\": \"" << u.name << "\"}";
        if (i < project.units.size() - 1) file << ",";
        file << "\n";
    }
    
    file << "  ]\n}\n";
    return true;
}

bool UnitManager::load_project(DesignProject& project, const std::string& path) {
    // Simple JSON parsing - in production use proper JSON library
    std::ifstream file(path);
    if (!file) return false;
    
    // TODO: Implement proper parsing
    return true;
}

} // namespace flexui_designer

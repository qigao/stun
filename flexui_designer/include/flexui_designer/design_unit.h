/*
 * flexUI Designer - Design Unit
 *
 * A unit combines layout + code, similar to Delphi's Form/Unit concept.
 * Each unit represents a reusable UI component with its associated logic.
 */

#pragma once

#include "designer.h"
#include <string>
#include <vector>
#include <map>

namespace flexui_designer {

// Widget group - multiple widgets treated as one
struct WidgetGroup {
    std::string id;
    std::string name;
    std::vector<std::string> widget_ids;
    bool locked = false;
    bool collapsed = false;  // In widget tree
};

// A design unit = layout + code
struct DesignUnit {
    std::string name;           // Unit name (e.g., "MainWindow")
    std::string layout_path;    // .fuid file
    std::string header_path;    // .h file
    std::string source_path;    // .cpp file
    std::string style_path;     // .css file (optional)
    
    std::vector<DesignWidget> widgets;
    std::vector<WidgetGroup> groups;
    
    // Event handlers defined in this unit
    struct EventHandler {
        std::string widget_id;
        std::string event_name;  // "on_click", "on_change", etc.
        std::string handler_name;
    };
    std::vector<EventHandler> handlers;
};

// Project containing multiple units
struct DesignProject {
    std::string name;
    std::string path;
    std::string main_unit;  // Entry point unit
    std::vector<DesignUnit> units;
    
    // Project-wide settings
    std::string output_dir;
    std::string namespace_name;
};

class UnitManager {
public:
    // Unit operations
    DesignUnit* create_unit(const std::string& name);
    DesignUnit* get_unit(const std::string& name);
    bool delete_unit(const std::string& name);
    bool rename_unit(const std::string& old_name, const std::string& new_name);
    
    // Group operations
    std::string create_group(DesignUnit& unit, const std::vector<std::string>& widget_ids, const std::string& name = "");
    bool ungroup(DesignUnit& unit, const std::string& group_id);
    WidgetGroup* get_group(DesignUnit& unit, const std::string& group_id);
    
    // Lock operations
    void lock_widget(DesignWidget& w) { w.locked = true; }
    void unlock_widget(DesignWidget& w) { w.locked = false; }
    void lock_group(WidgetGroup& g) { g.locked = true; }
    void unlock_group(WidgetGroup& g) { g.locked = false; }
    
    // Project operations
    bool save_project(const DesignProject& project, const std::string& path);
    bool load_project(DesignProject& project, const std::string& path);
    
    // Code generation
    std::string generate_header(const DesignUnit& unit);
    std::string generate_source(const DesignUnit& unit);
    
    const DesignProject& project() const { return project_; }
    DesignProject& project() { return project_; }

private:
    DesignProject project_;
    int next_group_id_ = 1;
};

} // namespace flexui_designer

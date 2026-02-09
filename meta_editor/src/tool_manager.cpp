/*
 * Meta Editor - Tool Manager Implementation
 */

#include "meta_editor/tool_manager.h"

namespace meta_editor {

ToolManager::ToolManager(Canvas* canvas, SelectionManager* selection, CommandManager* commands)
    : canvas_(canvas), selection_(selection), commands_(commands) {}

void ToolManager::set_target(Canvas* canvas, SelectionManager* selection, CommandManager* commands) {
    canvas_ = canvas;
    selection_ = selection;
    commands_ = commands;
    
    for (auto& pair : tools_) {
        pair.second->canvas_ = canvas;
        pair.second->selection_ = selection;
        pair.second->commands_ = commands;
    }
}

void ToolManager::register_tool(std::unique_ptr<Tool> tool) {
    tool->canvas_ = canvas_;
    tool->selection_ = selection_;
    tool->commands_ = commands_;
    tool->tool_manager_ = this;
    std::string name = tool->name();
    tools_[name] = std::move(tool);
}

void ToolManager::set_active_tool(const std::string& tool_name) {
    // Deactivate current tool
    if (active_tool_) {
        active_tool_->deactivate();
    }
    
    // Activate new tool
    auto it = tools_.find(tool_name);
    if (it != tools_.end()) {
        active_tool_ = it->second.get();
        active_tool_->activate();
    } else {
        active_tool_ = nullptr;
    }
}

Tool* ToolManager::get_tool(const std::string& name) const {
    auto it = tools_.find(name);
    return it != tools_.end() ? it->second.get() : nullptr;
}

std::vector<std::string> ToolManager::get_tool_names() const {
    std::vector<std::string> names;
    for (const auto& pair : tools_) {
        names.push_back(pair.first);
    }
    return names;
}

bool ToolManager::on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    return active_tool_ ? active_tool_->on_pointer_down(screen_pos, world_pos) : false;
}

bool ToolManager::on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    return active_tool_ ? active_tool_->on_pointer_move(screen_pos, world_pos) : false;
}

bool ToolManager::on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    return active_tool_ ? active_tool_->on_pointer_up(screen_pos, world_pos) : false;
}

bool ToolManager::on_key_down(int key, int mods) {
    return active_tool_ ? active_tool_->on_key_down(key, mods) : false;
}

bool ToolManager::on_key_up(int key, int mods) {
    return active_tool_ ? active_tool_->on_key_up(key, mods) : false;
}

bool ToolManager::on_text_input(const char* text) {
    return active_tool_ ? active_tool_->on_text_input(text) : false;
}

void ToolManager::render_overlay(flex::Renderer& renderer) {
    if (active_tool_) {
        active_tool_->render_overlay(renderer);
    }
}

void ToolManager::render_screen_overlay(flex::Renderer& renderer) {
    if (active_tool_) {
        active_tool_->render_screen_overlay(renderer);
    }
}

void ToolManager::update(float dt) {
    if (active_tool_) {
        active_tool_->update(dt);
    }
}

} // namespace meta_editor

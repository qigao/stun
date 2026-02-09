/*
 * Meta Editor - Tool Manager
 *
 * Manages active tool and tool switching.
 */

#pragma once

#include "tool.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

namespace meta_editor {

class ToolManager {
public:
    ToolManager(Canvas* canvas, SelectionManager* selection, CommandManager* commands);

    void set_target(Canvas* canvas, SelectionManager* selection, CommandManager* commands);

    void register_tool(std::unique_ptr<Tool> tool);

    void set_active_tool(const std::string& tool_name);
    Tool* active_tool() const { return active_tool_; }

    Canvas* canvas() const { return canvas_; }
    SelectionManager* selection() const { return selection_; }
    CommandManager* command_manager() const { return commands_; }

    Tool* get_tool(const std::string& name) const;
    std::vector<std::string> get_tool_names() const;

    bool on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos);
    bool on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos);
    bool on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos);
    bool on_key_down(int key, int mods);
    bool on_key_up(int key, int mods);
    bool on_text_input(const char* text);

    void render_overlay(flex::Renderer& renderer);
    void render_screen_overlay(flex::Renderer& renderer);
    void update(float dt);

private:
    Canvas* canvas_;
    SelectionManager* selection_;
    CommandManager* commands_;

    std::unordered_map<std::string, std::unique_ptr<Tool>> tools_;
    Tool* active_tool_ = nullptr;
};

} // namespace meta_editor

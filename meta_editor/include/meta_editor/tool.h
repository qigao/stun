/*
 * Meta Editor - Tool System
 *
 * Base class for editing tools (select, pen, shape, etc.)
 */

#pragma once

#include <flex.h>
#include <string>

namespace meta_editor {

class Canvas;
class SelectionManager;
class CommandManager;

class Tool {
public:
    virtual ~Tool() = default;

    virtual void activate() {}
    virtual void deactivate() {}

    virtual bool on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) { return false; }
    virtual bool on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) { return false; }
    virtual bool on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) { return false; }
    virtual bool on_key_down(int key, int mods) { return false; }
    virtual bool on_key_up(int key, int mods) { return false; }
    virtual bool on_text_input(const char* text) { return false; }

    virtual void render_overlay(flex::Renderer& renderer) {}
    // Screen-space overlay (selection handles, fixed-pixel UI)
    // Not affected by camera transform - coordinates are screen pixels
    virtual void render_screen_overlay(flex::Renderer& renderer) {}
    virtual void update(float dt) {}

    virtual const char* name() const = 0;
    virtual const char* icon() const { return ""; }
    virtual const char* cursor() const { return "arrow"; }

protected:
    Canvas* canvas_ = nullptr;
    SelectionManager* selection_ = nullptr;
    CommandManager* commands_ = nullptr;
    class ToolManager* tool_manager_ = nullptr;

    friend class ToolManager;
};

} // namespace meta_editor

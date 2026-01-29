/*
 * Meta Editor - Context Menu
 *
 * Right-click popup menu for common operations.
 */

#pragma once

#include "panel.h"
#include <functional>
#include <string>
#include <vector>

namespace meta_editor {

class Editor;

struct MenuItem {
    std::string label;
    std::string shortcut;
    std::function<void()> action;
    bool enabled = true;
    bool separator = false;
};

class ContextMenu : public Panel {
public:
    explicit ContextMenu(Editor* editor);

    void render(flex::Renderer& renderer) override;
    bool handle_click(float px, float py) override;

    void show(float x, float y);
    void hide() { set_visible(false); }

    void set_items(std::vector<MenuItem> items) { items_ = std::move(items); }

protected:
    float content_height() const override;

private:
    Editor* editor_;
    std::vector<MenuItem> items_;
    int hovered_index_ = -1;
    
    static constexpr float ITEM_HEIGHT = 28.0f;
    static constexpr float PADDING = 8.0f;
};

} // namespace meta_editor

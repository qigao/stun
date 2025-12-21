/*
 * Context Menu
 *
 * Right-click context menu for canvas and selection actions.
 * Dynamically builds menu items based on selection state.
 */

#pragma once

#include "../core/types.h"
#include <flex/flex.h>
#include <string>
#include <vector>
#include <functional>

namespace editor {

// Forward declaration
class EditorViewModel;

// Menu item definition
struct MenuItem {
    std::string id;
    std::string label;
    std::string shortcut;      // Display shortcut hint
    bool separator = false;    // Is this a separator?
    bool enabled = true;
    bool checked = false;      // For toggle items
    std::vector<MenuItem> submenu;  // For nested menus

    MenuItem() = default;
    MenuItem(const std::string& id_, const std::string& label_, const std::string& sc = "")
        : id(id_), label(label_), shortcut(sc) {}

    static MenuItem Separator() {
        MenuItem item;
        item.separator = true;
        return item;
    }

    bool hasSubmenu() const { return !submenu.empty(); }
};

// Context menu style
struct ContextMenuStyle {
    float item_height = 24;
    float min_width = 160;
    float padding = 4;
    float submenu_offset = 4;
    float separator_height = 8;
    float shortcut_margin = 40;

    Color background = {0.2f, 0.2f, 0.2f, 0.98f};
    Color item_hover = {0.3f, 0.5f, 0.8f, 1.0f};
    Color text_normal = {0.9f, 0.9f, 0.9f, 1.0f};
    Color text_disabled = {0.5f, 0.5f, 0.5f, 1.0f};
    Color shortcut_text = {0.6f, 0.6f, 0.6f, 1.0f};
    Color separator_color = {0.35f, 0.35f, 0.35f, 1.0f};
    Color border = {0.35f, 0.35f, 0.35f, 1.0f};
};

// Context Menu component
class ContextMenu {
public:
    ContextMenu();

    void setViewModel(EditorViewModel* vm) { vm_ = vm; }

    // Show/hide
    void show(float x, float y);
    void hide();
    bool isVisible() const { return visible_; }

    // Position
    float x() const { return x_; }
    float y() const { return y_; }
    float width() const { return calculated_width_; }
    float height() const { return calculated_height_; }
    Rect bounds() const { return {x_, y_, calculated_width_, calculated_height_}; }

    // Menu items
    void setItems(const std::vector<MenuItem>& items);
    void buildForSelection();   // Build menu based on current selection
    void buildForCanvas();      // Build menu for canvas (no selection)

    // Callbacks
    void onItemSelected(std::function<void(const std::string&)> cb) { onItemSelected_ = std::move(cb); }

    // Style
    ContextMenuStyle& style() { return style_; }

    // Rendering
    void render(flex::Renderer& renderer);

    // Input handling
    bool onMouseDown(float mx, float my, int button);
    bool onMouseMove(float mx, float my);
    bool onMouseUp(float mx, float my, int button);

private:
    void calculateSize();
    void renderItem(flex::Renderer& renderer, const MenuItem& item, float& y, int index);
    void renderSubmenu(flex::Renderer& renderer, const std::vector<MenuItem>& submenu);
    int itemAt(float mx, float my) const;
    int submenuItemAt(float mx, float my) const;

    EditorViewModel* vm_ = nullptr;
    float x_ = 0, y_ = 0;
    float calculated_width_ = 160;
    float calculated_height_ = 100;
    bool visible_ = false;

    std::vector<MenuItem> items_;
    int hovered_item_ = -1;

    // Submenu state
    int submenu_parent_ = -1;
    float submenu_x_ = 0, submenu_y_ = 0;
    int submenu_hovered_ = -1;

    ContextMenuStyle style_;
    std::function<void(const std::string&)> onItemSelected_;
};

} // namespace editor

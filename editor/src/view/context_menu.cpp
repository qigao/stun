/*
 * Context Menu Implementation
 */

#include <editor/view/context_menu.h>
#include <editor/viewmodel/editor_vm.h>
#include <algorithm>
#include <cstdio>

namespace editor {

ContextMenu::ContextMenu() {}

void ContextMenu::show(float x, float y) {
    x_ = x;
    y_ = y;
    visible_ = true;
    hovered_item_ = -1;
    submenu_parent_ = -1;
    submenu_hovered_ = -1;
    calculateSize();
}

void ContextMenu::hide() {
    visible_ = false;
    hovered_item_ = -1;
    submenu_parent_ = -1;
}

void ContextMenu::setItems(const std::vector<MenuItem>& items) {
    items_ = items;
    calculateSize();
}

void ContextMenu::buildForSelection() {
    items_.clear();

    if (!vm_) return;

    auto& sel = vm_->selection();
    bool hasSelection = !sel.isEmpty();
    bool multiSelection = sel.count() > 1;

    // Edit operations
    items_.push_back({"edit.cut", "Cut", "Ctrl+X"});
    items_.back().enabled = hasSelection;

    items_.push_back({"edit.copy", "Copy", "Ctrl+C"});
    items_.back().enabled = hasSelection;

    items_.push_back({"edit.paste", "Paste", "Ctrl+V"});

    items_.push_back({"edit.duplicate", "Duplicate", "Ctrl+D"});
    items_.back().enabled = hasSelection;

    items_.push_back(MenuItem::Separator());

    items_.push_back({"edit.delete", "Delete", "Del"});
    items_.back().enabled = hasSelection;

    items_.push_back(MenuItem::Separator());

    // Arrange submenu
    MenuItem arrange("arrange", "Arrange");
    arrange.submenu = {
        {"arrange.bring_front", "Bring to Front", "Ctrl+Shift+]"},
        {"arrange.bring_forward", "Bring Forward", "Ctrl+]"},
        {"arrange.send_backward", "Send Backward", "Ctrl+["},
        {"arrange.send_back", "Send to Back", "Ctrl+Shift+["}
    };
    for (auto& item : arrange.submenu) {
        item.enabled = hasSelection;
    }
    items_.push_back(arrange);

    // Transform submenu
    MenuItem transform("transform", "Transform");
    transform.submenu = {
        {"transform.rotate_cw", "Rotate 90° CW"},
        {"transform.rotate_ccw", "Rotate 90° CCW"},
        {"transform.flip_h", "Flip Horizontal"},
        {"transform.flip_v", "Flip Vertical"}
    };
    for (auto& item : transform.submenu) {
        item.enabled = hasSelection;
    }
    items_.push_back(transform);

    items_.push_back(MenuItem::Separator());

    // Group operations
    items_.push_back({"object.group", "Group", "Ctrl+G"});
    items_.back().enabled = multiSelection;

    items_.push_back({"object.ungroup", "Ungroup", "Ctrl+Shift+G"});
    items_.back().enabled = hasSelection;

    items_.push_back(MenuItem::Separator());

    // Boolean operations (for multiple shapes)
    MenuItem boolOps("boolean", "Boolean");
    boolOps.submenu = {
        {"object.bool_union", "Union", "Ctrl+U"},
        {"object.bool_subtract", "Subtract", "Ctrl+["},
        {"object.bool_intersect", "Intersect", "Ctrl+Shift+I"},
        {"object.bool_exclude", "Exclude"}
    };
    for (auto& item : boolOps.submenu) {
        item.enabled = multiSelection;
    }
    items_.push_back(boolOps);

    calculateSize();
}

void ContextMenu::buildForCanvas() {
    items_.clear();

    // Canvas context menu (no selection)
    items_.push_back({"edit.paste", "Paste", "Ctrl+V"});
    items_.push_back({"edit.paste_here", "Paste Here"});

    items_.push_back(MenuItem::Separator());

    items_.push_back({"edit.select_all", "Select All", "Ctrl+A"});

    items_.push_back(MenuItem::Separator());

    // View operations
    MenuItem view("view", "View");
    view.submenu = {
        {"view.zoom_in", "Zoom In", "Ctrl++"},
        {"view.zoom_out", "Zoom Out", "Ctrl+-"},
        {"view.zoom_reset", "Zoom 100%", "Ctrl+0"},
        {"view.zoom_fit", "Zoom to Fit", "Ctrl+1"}
    };
    items_.push_back(view);

    items_.push_back(MenuItem::Separator());

    // Grid/guides
    items_.push_back({"view.toggle_grid", "Show Grid"});
    items_.push_back({"view.toggle_guides", "Show Guides"});
    items_.push_back({"view.toggle_rulers", "Show Rulers"});

    calculateSize();
}

void ContextMenu::calculateSize() {
    float maxWidth = style_.min_width;
    float totalHeight = style_.padding * 2;

    for (const auto& item : items_) {
        if (item.separator) {
            totalHeight += style_.separator_height;
        } else {
            totalHeight += style_.item_height;

            // Estimate text width
            float textWidth = item.label.length() * 7 + style_.padding * 2;
            if (!item.shortcut.empty()) {
                textWidth += style_.shortcut_margin + item.shortcut.length() * 6;
            }
            if (item.hasSubmenu()) {
                textWidth += 20;  // Arrow indicator
            }
            maxWidth = std::max(maxWidth, textWidth);
        }
    }

    calculated_width_ = maxWidth;
    calculated_height_ = totalHeight;
}

void ContextMenu::render(flex::Renderer& renderer) {
    if (!visible_) return;

    // Background with border
    std::string bg = "M " + std::to_string(x_) + " " + std::to_string(y_) +
        " h " + std::to_string(calculated_width_) +
        " v " + std::to_string(calculated_height_) +
        " h " + std::to_string(-calculated_width_) + " Z";

    renderer.fill_path(bg, flex::Paint::solid({style_.background.r, style_.background.g,
        style_.background.b, style_.background.a}));
    renderer.stroke_path(bg, flex::Paint::solid({style_.border.r, style_.border.g,
        style_.border.b, 1}), 1.0f);

    // Render items
    float y = y_ + style_.padding;
    for (size_t i = 0; i < items_.size(); ++i) {
        renderItem(renderer, items_[i], y, static_cast<int>(i));
    }

    // Render submenu if open
    if (submenu_parent_ >= 0 && submenu_parent_ < static_cast<int>(items_.size())) {
        const auto& parent = items_[submenu_parent_];
        if (parent.hasSubmenu()) {
            renderSubmenu(renderer, parent.submenu);
        }
    }
}

void ContextMenu::renderSubmenu(flex::Renderer& renderer, const std::vector<MenuItem>& submenu) {
    // Calculate submenu size
    float subWidth = style_.min_width;
    float subHeight = style_.padding * 2;

    for (const auto& item : submenu) {
        if (item.separator) {
            subHeight += style_.separator_height;
        } else {
            subHeight += style_.item_height;
            float textWidth = item.label.length() * 7 + style_.padding * 2;
            if (!item.shortcut.empty()) {
                textWidth += style_.shortcut_margin + item.shortcut.length() * 6;
            }
            subWidth = std::max(subWidth, textWidth);
        }
    }

    // Background
    std::string bg = "M " + std::to_string(submenu_x_) + " " + std::to_string(submenu_y_) +
        " h " + std::to_string(subWidth) +
        " v " + std::to_string(subHeight) +
        " h " + std::to_string(-subWidth) + " Z";

    renderer.fill_path(bg, flex::Paint::solid({style_.background.r, style_.background.g,
        style_.background.b, style_.background.a}));
    renderer.stroke_path(bg, flex::Paint::solid({style_.border.r, style_.border.g,
        style_.border.b, 1}), 1.0f);

    // Render submenu items
    float y = submenu_y_ + style_.padding;
    for (size_t i = 0; i < submenu.size(); ++i) {
        const auto& item = submenu[i];

        if (item.separator) {
            float sepY = y + style_.separator_height / 2;
            std::string sep = "M " + std::to_string(submenu_x_ + style_.padding) +
                " " + std::to_string(sepY) +
                " h " + std::to_string(subWidth - style_.padding * 2);
            renderer.stroke_path(sep, flex::Paint::solid({style_.separator_color.r,
                style_.separator_color.g, style_.separator_color.b, 1}), 1.0f);
            y += style_.separator_height;
        } else {
            bool hovered = (static_cast<int>(i) == submenu_hovered_);

            // Hover background
            if (hovered && item.enabled) {
                std::string hoverBg = "M " + std::to_string(submenu_x_ + 2) +
                    " " + std::to_string(y) +
                    " h " + std::to_string(subWidth - 4) +
                    " v " + std::to_string(style_.item_height) +
                    " h " + std::to_string(-(subWidth - 4)) + " Z";
                renderer.fill_path(hoverBg, flex::Paint::solid({style_.item_hover.r,
                    style_.item_hover.g, style_.item_hover.b, style_.item_hover.a}));
            }

            // Label
            Color textColor = item.enabled ? style_.text_normal : style_.text_disabled;
            renderer.draw_text(item.label, submenu_x_ + style_.padding + 4,
                y + style_.item_height - 7, "Arial", 11, false,
                {textColor.r, textColor.g, textColor.b, 1});

            // Shortcut
            if (!item.shortcut.empty()) {
                float scX = submenu_x_ + subWidth - style_.padding -
                    item.shortcut.length() * 6 - 4;
                renderer.draw_text(item.shortcut, scX, y + style_.item_height - 7,
                    "Arial", 10, false,
                    {style_.shortcut_text.r, style_.shortcut_text.g, style_.shortcut_text.b, 1});
            }

            y += style_.item_height;
        }
    }
}

void ContextMenu::renderItem(flex::Renderer& renderer, const MenuItem& item, float& y, int index) {
    if (item.separator) {
        float sepY = y + style_.separator_height / 2;
        std::string sep = "M " + std::to_string(x_ + style_.padding) + " " + std::to_string(sepY) +
            " h " + std::to_string(calculated_width_ - style_.padding * 2);
        renderer.stroke_path(sep, flex::Paint::solid({style_.separator_color.r,
            style_.separator_color.g, style_.separator_color.b, 1}), 1.0f);
        y += style_.separator_height;
        return;
    }

    bool hovered = (index == hovered_item_);

    // Hover background
    if (hovered && item.enabled) {
        std::string hoverBg = "M " + std::to_string(x_ + 2) + " " + std::to_string(y) +
            " h " + std::to_string(calculated_width_ - 4) +
            " v " + std::to_string(style_.item_height) +
            " h " + std::to_string(-(calculated_width_ - 4)) + " Z";
        renderer.fill_path(hoverBg, flex::Paint::solid({style_.item_hover.r, style_.item_hover.g,
            style_.item_hover.b, style_.item_hover.a}));
    }

    // Label
    Color textColor = item.enabled ? style_.text_normal : style_.text_disabled;
    renderer.draw_text(item.label, x_ + style_.padding + 4, y + style_.item_height - 7,
        "Arial", 11, false, {textColor.r, textColor.g, textColor.b, 1});

    // Shortcut hint
    if (!item.shortcut.empty()) {
        float scX = x_ + calculated_width_ - style_.padding - item.shortcut.length() * 6 - 4;
        renderer.draw_text(item.shortcut, scX, y + style_.item_height - 7, "Arial", 10, false,
            {style_.shortcut_text.r, style_.shortcut_text.g, style_.shortcut_text.b, 1});
    }

    // Submenu arrow
    if (item.hasSubmenu()) {
        float arrowX = x_ + calculated_width_ - style_.padding - 8;
        float arrowY = y + style_.item_height / 2;
        renderer.draw_text(">", arrowX, arrowY + 4, "Arial", 10, false,
            {textColor.r, textColor.g, textColor.b, 1});

        // Show submenu on hover
        if (hovered) {
            submenu_parent_ = index;
            submenu_x_ = x_ + calculated_width_ + style_.submenu_offset;
            submenu_y_ = y;
        }
    }

    y += style_.item_height;
}

int ContextMenu::itemAt(float mx, float my) const {
    if (mx < x_ || mx > x_ + calculated_width_ || my < y_ || my > y_ + calculated_height_) {
        return -1;
    }

    float y = y_ + style_.padding;
    for (size_t i = 0; i < items_.size(); ++i) {
        const auto& item = items_[i];
        float itemH = item.separator ? style_.separator_height : style_.item_height;

        if (my >= y && my < y + itemH) {
            return item.separator ? -1 : static_cast<int>(i);
        }
        y += itemH;
    }

    return -1;
}

int ContextMenu::submenuItemAt(float mx, float my) const {
    if (submenu_parent_ < 0 || submenu_parent_ >= static_cast<int>(items_.size())) {
        return -1;
    }

    const auto& parent = items_[submenu_parent_];
    if (!parent.hasSubmenu()) return -1;

    // Calculate submenu bounds
    float subWidth = style_.min_width;
    float subHeight = style_.padding * 2;

    for (const auto& item : parent.submenu) {
        subHeight += item.separator ? style_.separator_height : style_.item_height;
        if (!item.separator) {
            float textWidth = item.label.length() * 7 + style_.padding * 2;
            if (!item.shortcut.empty()) {
                textWidth += style_.shortcut_margin + item.shortcut.length() * 6;
            }
            subWidth = std::max(subWidth, textWidth);
        }
    }

    if (mx < submenu_x_ || mx > submenu_x_ + subWidth ||
        my < submenu_y_ || my > submenu_y_ + subHeight) {
        return -1;
    }

    float y = submenu_y_ + style_.padding;
    for (size_t i = 0; i < parent.submenu.size(); ++i) {
        const auto& item = parent.submenu[i];
        float itemH = item.separator ? style_.separator_height : style_.item_height;

        if (my >= y && my < y + itemH) {
            return item.separator ? -1 : static_cast<int>(i);
        }
        y += itemH;
    }

    return -1;
}

bool ContextMenu::onMouseDown(float mx, float my, int button) {
    if (!visible_) return false;

    // Check submenu first
    if (submenu_parent_ >= 0) {
        int subIdx = submenuItemAt(mx, my);
        if (subIdx >= 0) {
            const auto& parent = items_[submenu_parent_];
            const auto& item = parent.submenu[subIdx];
            if (item.enabled && onItemSelected_) {
                onItemSelected_(item.id);
            }
            hide();
            return true;
        }
    }

    // Check main menu
    int idx = itemAt(mx, my);
    if (idx >= 0) {
        const auto& item = items_[idx];
        if (item.enabled && !item.hasSubmenu() && onItemSelected_) {
            onItemSelected_(item.id);
            hide();
        }
        return true;
    }

    // Click outside - close menu
    hide();
    return false;
}

bool ContextMenu::onMouseMove(float mx, float my) {
    if (!visible_) return false;

    // Check submenu
    if (submenu_parent_ >= 0) {
        submenu_hovered_ = submenuItemAt(mx, my);
    }

    // Check main menu
    int idx = itemAt(mx, my);
    if (idx != hovered_item_) {
        hovered_item_ = idx;

        // Close submenu if hovering different item
        if (idx >= 0 && !items_[idx].hasSubmenu()) {
            submenu_parent_ = -1;
            submenu_hovered_ = -1;
        }
    }

    return hovered_item_ >= 0 || submenu_hovered_ >= 0;
}

bool ContextMenu::onMouseUp(float mx, float my, int button) {
    (void)mx; (void)my; (void)button;
    return visible_;
}

} // namespace editor

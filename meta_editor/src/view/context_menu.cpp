/*
 * Meta Editor - Context Menu Implementation
 */

#include "meta_editor/view/context_menu.h"
#include "meta_editor/core/editor.h"

namespace meta_editor {

ContextMenu::ContextMenu(Editor* editor) : editor_(editor) {
    set_size(180, 200);
    set_draggable(false);
    set_visible(false);
}

void ContextMenu::show(float x, float y) {
    set_position(x, y);
    set_visible(true);
    hovered_index_ = -1;

    // Build default menu items based on selection state
    items_.clear();
    
    bool has_sel = editor_->selection()->has_selection();
    
    items_.push_back({"Cut", "Ctrl+X", [this]() {
        editor_->copy_selection();
        editor_->delete_selection();
        hide();
    }, has_sel});
    
    items_.push_back({"Copy", "Ctrl+C", [this]() {
        editor_->copy_selection();
        hide();
    }, has_sel});
    
    items_.push_back({"Paste", "Ctrl+V", [this]() {
        editor_->paste();
        hide();
    }, editor_->has_clipboard()});
    
    items_.push_back({"Duplicate", "Ctrl+D", [this]() {
        editor_->duplicate_selection();
        hide();
    }, has_sel});
    
    items_.push_back({"", "", nullptr, false, true});  // Separator
    
    items_.push_back({"Delete", "Del", [this]() {
        editor_->delete_selection();
        hide();
    }, has_sel});
    
    items_.push_back({"", "", nullptr, false, true});  // Separator
    
    items_.push_back({"Select All", "Ctrl+A", [this]() {
        editor_->select_all();
        hide();
    }, true});
    
    if (has_sel && editor_->selection()->selection_count() > 1) {
        items_.push_back({"Group", "Ctrl+G", [this]() {
            editor_->group_selection();
            hide();
        }, true});
    }
    
    if (has_sel && editor_->selection()->selection_count() == 1) {
        auto* node = editor_->selection()->primary_selection();
        if (node && node->type() == flex::NodeType::Group) {
            items_.push_back({"Ungroup", "Ctrl+Shift+G", [this]() {
                editor_->ungroup_selection();
                hide();
            }, true});
        }
    }
}

float ContextMenu::content_height() const {
    float h = PADDING * 2;
    for (const auto& item : items_) {
        h += item.separator ? 9.0f : ITEM_HEIGHT;
    }
    return h;
}

void ContextMenu::render(flex::Renderer& renderer) {
    if (!visible_) return;

    float h = content_height();
    
    // Background
    flex::Paint bg = flex::Paint::solid(flex::Color{0.18f, 0.18f, 0.2f, 0.98f});
    flex::Paint border = flex::Paint::solid(flex::Color{0.35f, 0.35f, 0.38f, 1.0f});
    renderer.draw_rect(x_, y_, width_, h, 6.0f, bg, border, 1.0f);

    float item_y = y_ + PADDING;
    
    for (size_t i = 0; i < items_.size(); ++i) {
        const auto& item = items_[i];
        
        if (item.separator) {
            flex::Paint sep = flex::Paint::solid(flex::Color{0.35f, 0.35f, 0.38f, 0.6f});
            renderer.draw_rect(x_ + 8, item_y + 4, width_ - 16, 1, 0, sep, flex::Paint::none(), 0);
            item_y += 9.0f;
            continue;
        }

        bool hovered = (static_cast<int>(i) == hovered_index_);
        
        if (hovered && item.enabled) {
            flex::Paint hover_bg = flex::Paint::solid(flex::Color{0.3f, 0.5f, 0.8f, 0.8f});
            renderer.draw_rect(x_ + 4, item_y, width_ - 8, ITEM_HEIGHT, 4.0f, 
                              hover_bg, flex::Paint::none(), 0);
        }

        flex::Color text_color = item.enabled 
            ? flex::Color{1.0f, 1.0f, 1.0f, 1.0f}
            : flex::Color{0.5f, 0.5f, 0.5f, 1.0f};

        renderer.draw_text(item.label.c_str(), x_ + 12, item_y + 18, 
                          "Arial", 12.0f, false, text_color);

        if (!item.shortcut.empty()) {
            flex::Color shortcut_color = item.enabled
                ? flex::Color{0.6f, 0.6f, 0.6f, 1.0f}
                : flex::Color{0.4f, 0.4f, 0.4f, 1.0f};
            renderer.draw_text(item.shortcut.c_str(), x_ + width_ - 60, item_y + 18,
                              "Arial", 10.0f, false, shortcut_color);
        }

        item_y += ITEM_HEIGHT;
    }
}

bool ContextMenu::handle_click(float px, float py) {
    float item_y = y_ + PADDING;
    
    for (size_t i = 0; i < items_.size(); ++i) {
        const auto& item = items_[i];
        
        if (item.separator) {
            item_y += 9.0f;
            continue;
        }

        if (py >= item_y && py < item_y + ITEM_HEIGHT) {
            if (item.enabled && item.action) {
                item.action();
            }
            return true;
        }

        item_y += ITEM_HEIGHT;
    }

    return false;
}

} // namespace meta_editor

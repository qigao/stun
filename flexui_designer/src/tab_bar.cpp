/*
 * flexUI Designer - Tab Bar Implementation
 */

#include "flexui_designer/tab_bar.h"
#include <algorithm>
#include <cmath>

namespace flexui_designer {

void TabBar::add_tab(const std::string& id, const std::string& title) {
    for (const auto& t : tabs_) {
        if (t.id == id) return;
    }
    tabs_.push_back({id, title, false, true});
    if (active_id_.empty()) active_id_ = id;
}

void TabBar::remove_tab(const std::string& id) {
    auto it = std::find_if(tabs_.begin(), tabs_.end(),
        [&](const TabInfo& t) { return t.id == id; });
    if (it == tabs_.end()) return;
    
    size_t idx = it - tabs_.begin();
    tabs_.erase(it);
    
    if (active_id_ == id) {
        if (tabs_.empty()) {
            active_id_.clear();
        } else {
            active_id_ = tabs_[std::min(idx, tabs_.size() - 1)].id;
        }
        if (on_select_ && !active_id_.empty()) on_select_(active_id_);
    }
}

void TabBar::set_active(const std::string& id) {
    for (const auto& t : tabs_) {
        if (t.id == id) { active_id_ = id; return; }
    }
}

void TabBar::set_modified(const std::string& id, bool modified) {
    for (auto& t : tabs_) {
        if (t.id == id) { t.modified = modified; return; }
    }
}

void TabBar::rename_tab(const std::string& id, const std::string& new_title) {
    for (auto& t : tabs_) {
        if (t.id == id) { t.title = new_title; return; }
    }
}

void TabBar::move_tab(int from_index, int to_index) {
    if (from_index < 0 || from_index >= (int)tabs_.size()) return;
    if (to_index < 0 || to_index >= (int)tabs_.size()) return;
    if (from_index == to_index) return;
    
    TabInfo tab = tabs_[from_index];
    tabs_.erase(tabs_.begin() + from_index);
    tabs_.insert(tabs_.begin() + to_index, tab);
}

float TabBar::get_tab_width() const {
    float w = std::min(TAB_WIDTH, (width_ - 50) / std::max(1.0f, (float)tabs_.size()));
    return std::max(w, TAB_MIN_WIDTH);
}

void TabBar::render(flex::Renderer& renderer) {
    // Background
    renderer.draw_rect(x_, y_, width_, height_, 0,
        flex::Paint::solid(flex::Color{0.1f, 0.1f, 0.12f, 1}), flex::Paint::none(), 0);
    renderer.draw_rect(x_, y_ + height_ - 1, width_, 1, 0,
        flex::Paint::solid(flex::Color{0.2f, 0.2f, 0.22f, 1}), flex::Paint::none(), 0);
    
    float tab_w = get_tab_width();
    float tx = x_;
    
    for (size_t i = 0; i < tabs_.size(); ++i) {
        const auto& tab = tabs_[i];
        bool active = (tab.id == active_id_);
        bool is_dragging_this = dragging_ && (int)i == drag_tab_index_;
        
        float draw_x = tx;
        if (is_dragging_this) {
            draw_x = drag_current_x_ - drag_offset_x_;
            draw_x = std::max(x_, std::min(draw_x, x_ + width_ - tab_w));
        }
        
        // Tab background
        flex::Color bg = active ? flex::Color{0.18f, 0.18f, 0.2f, 1}
                                : flex::Color{0.12f, 0.12f, 0.14f, 1};
        if (is_dragging_this) bg = flex::Color{0.22f, 0.22f, 0.25f, 1};
        
        renderer.draw_rect(draw_x, y_, tab_w - 1, height_, 0,
            flex::Paint::solid(bg), flex::Paint::none(), 0);
        
        if (active) {
            renderer.draw_rect(draw_x, y_ + height_ - 2, tab_w - 1, 2, 0,
                flex::Paint::solid(flex::Color{0.3f, 0.6f, 1.0f, 1}), flex::Paint::none(), 0);
        }
        
        // Title with modified indicator
        std::string display = tab.modified ? "• " + tab.title : tab.title;
        if (display.length() > 15) display = display.substr(0, 12) + "...";
        
        flex::Color text_col = active ? flex::Color{1, 1, 1, 1}
                                      : flex::Color{0.6f, 0.6f, 0.65f, 1};
        renderer.draw_text(display, draw_x + 10, y_ + 18, "sans", 11, false, text_col);
        
        // Close button
        if (tab.closable) {
            renderer.draw_text("×", draw_x + tab_w - 20, y_ + height_ / 2 + 5, 
                "sans", 14, false, {0.5f, 0.5f, 0.55f, 1});
        }
        
        tx += tab_w;
    }
    
    // New tab button
    renderer.draw_rect(tx + 5, y_ + 4, 20, 20, 4,
        flex::Paint::solid(flex::Color{0.15f, 0.15f, 0.18f, 1}), flex::Paint::none(), 0);
    renderer.draw_text("+", tx + 10, y_ + 18, "sans", 14, false, {0.5f, 0.5f, 0.55f, 1});
    
    // Drop indicator when dragging
    if (dragging_) {
        int target = hit_test_tab(drag_current_x_, y_ + height_ / 2);
        if (target >= 0 && target != drag_tab_index_) {
            float indicator_x = x_ + target * tab_w;
            if (target > drag_tab_index_) indicator_x += tab_w;
            renderer.draw_rect(indicator_x - 1, y_ + 2, 2, height_ - 4, 1,
                flex::Paint::solid(flex::Color{0.3f, 0.6f, 1.0f, 1}), flex::Paint::none(), 0);
        }
    }
    
    // Context menu
    if (context_menu_visible_) render_context_menu(renderer);
}

bool TabBar::handle_click(float x, float y, bool right_click) {
    // Handle context menu click first
    if (context_menu_visible_) {
        if (handle_context_menu_click(x, y)) return true;
        context_menu_visible_ = false;
        return true;
    }
    
    if (y < y_ || y > y_ + height_) return false;
    
    float tab_w = get_tab_width();
    
    // Right click - show context menu
    if (right_click) {
        int idx = hit_test_tab(x, y);
        if (idx >= 0) {
            show_context_menu(idx, x, y + height_);
            return true;
        }
        return false;
    }
    
    // New tab button
    float new_btn_x = x_ + tabs_.size() * tab_w + 5;
    if (x >= new_btn_x && x < new_btn_x + 20) {
        if (on_new_tab_) on_new_tab_();
        return true;
    }
    
    int idx = hit_test_tab(x, y);
    if (idx < 0 || idx >= (int)tabs_.size()) return false;
    
    // Close button
    if (hit_test_close(x, y, idx)) {
        if (on_close_) on_close_(tabs_[idx].id);
        return true;
    }
    
    // Start drag
    dragging_ = true;
    drag_tab_index_ = idx;
    drag_start_x_ = x;
    drag_offset_x_ = x - (x_ + idx * tab_w);
    drag_current_x_ = x;
    
    // Select tab
    if (tabs_[idx].id != active_id_) {
        active_id_ = tabs_[idx].id;
        if (on_select_) on_select_(active_id_);
    }
    return true;
}

bool TabBar::handle_drag(float x, float y) {
    if (!dragging_) return false;
    drag_current_x_ = x;
    return true;
}

bool TabBar::handle_drop(float x, float y) {
    if (!dragging_) return false;
    
    // Check if actually dragged (not just clicked)
    if (std::abs(x - drag_start_x_) > 5) {
        int target = hit_test_tab(x, y_ + height_ / 2);
        if (target >= 0 && target != drag_tab_index_) {
            move_tab(drag_tab_index_, target);
        }
    }
    
    dragging_ = false;
    drag_tab_index_ = -1;
    return true;
}

void TabBar::cancel_drag() {
    dragging_ = false;
    drag_tab_index_ = -1;
}

int TabBar::hit_test_tab(float x, float y) const {
    if (y < y_ || y > y_ + height_) return -1;
    float tab_w = get_tab_width();
    int idx = (int)((x - x_) / tab_w);
    return (idx >= 0 && idx < (int)tabs_.size()) ? idx : -1;
}

bool TabBar::hit_test_close(float x, float y, int tab_index) const {
    if (tab_index < 0 || tab_index >= (int)tabs_.size()) return false;
    if (!tabs_[tab_index].closable) return false;
    float tab_w = get_tab_width();
    float close_x = x_ + tab_index * tab_w + tab_w - 24;
    return (x >= close_x && x < close_x + 18);
}

void TabBar::show_context_menu(int tab_index, float x, float y) {
    context_menu_tab_index_ = tab_index;
    context_menu_x_ = x;
    context_menu_y_ = y;
    context_menu_visible_ = true;
    
    context_menu_items_.clear();
    const auto& tab = tabs_[tab_index];
    
    context_menu_items_.push_back({"Rename", [this, tab]() {
        if (on_rename_) on_rename_(tab.id);
    }});
    
    context_menu_items_.push_back({"", nullptr, true});
    
    context_menu_items_.push_back({"Close", [this, tab]() {
        if (on_close_) on_close_(tab.id);
    }});
    
    if (tabs_.size() > 1) {
        context_menu_items_.push_back({"Close Others", [this, tab]() {
            std::vector<std::string> to_close;
            for (const auto& t : tabs_) {
                if (t.id != tab.id) to_close.push_back(t.id);
            }
            for (const auto& id : to_close) {
                if (on_close_) on_close_(id);
            }
        }});
        
        context_menu_items_.push_back({"Close All", [this]() {
            std::vector<std::string> to_close;
            for (const auto& t : tabs_) to_close.push_back(t.id);
            for (const auto& id : to_close) {
                if (on_close_) on_close_(id);
            }
        }});
    }
}

void TabBar::render_context_menu(flex::Renderer& renderer) {
    constexpr float ITEM_H = 26.0f;
    constexpr float PADDING = 6.0f;
    constexpr float WIDTH = 140.0f;
    
    float menu_height = PADDING * 2;
    for (const auto& item : context_menu_items_) {
        menu_height += item.separator ? 8.0f : ITEM_H;
    }
    
    renderer.draw_rect(context_menu_x_, context_menu_y_, WIDTH, menu_height, 6,
        flex::Paint::solid(flex::Color{0.18f, 0.18f, 0.2f, 0.98f}),
        flex::Paint::solid(flex::Color{0.3f, 0.3f, 0.35f, 1}), 1);
    
    float item_y = context_menu_y_ + PADDING;
    for (const auto& item : context_menu_items_) {
        if (item.separator) {
            renderer.draw_rect(context_menu_x_ + 8, item_y + 3, WIDTH - 16, 1, 0,
                flex::Paint::solid(flex::Color{0.35f, 0.35f, 0.4f, 1}), flex::Paint::none(), 0);
            item_y += 8.0f;
        } else {
            renderer.draw_text(item.label, context_menu_x_ + 12, item_y + 17, 
                "sans", 12, false, {0.9f, 0.9f, 0.9f, 1});
            item_y += ITEM_H;
        }
    }
}

bool TabBar::handle_context_menu_click(float x, float y) {
    constexpr float ITEM_H = 26.0f;
    constexpr float PADDING = 6.0f;
    constexpr float WIDTH = 140.0f;
    
    if (x < context_menu_x_ || x > context_menu_x_ + WIDTH) {
        context_menu_visible_ = false;
        return false;
    }
    
    float item_y = context_menu_y_ + PADDING;
    for (const auto& item : context_menu_items_) {
        float h = item.separator ? 8.0f : ITEM_H;
        if (!item.separator && y >= item_y && y < item_y + h) {
            if (item.action) item.action();
            context_menu_visible_ = false;
            return true;
        }
        item_y += h;
    }
    
    context_menu_visible_ = false;
    return false;
}

} // namespace flexui_designer

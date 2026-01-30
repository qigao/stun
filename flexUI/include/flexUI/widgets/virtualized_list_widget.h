/*
 * flexUI - VirtualizedListWidget
 *
 * A high-performance list widget that uses DOM virtualization.
 * It manages a pool of child Elements and recycles them to render
 * only the visible rows. This allows for lists with 10,000+ items
 * while keeping the DOM size small (e.g., only 20 elements).
 */

#ifndef FLEXUI_VIRTUALIZED_LIST_WIDGET_H
#define FLEXUI_VIRTUALIZED_LIST_WIDGET_H

#include "../widget.h"
#include "../box.h"
#include "../renderer.h"
#include <vector>
#include <deque>
#include <functional>
#include <cmath>
#include <algorithm>

namespace flexUI {

class VirtualizedListWidget : public Widget {
public:
    using CreateRowFn = std::function<Element*()>;
    using BindRowFn = std::function<void(Element* row, int index)>;

    VirtualizedListWidget() = default;
    
    // Configuration
    void set_item_count(int count) {
        if (count_ != count) {
            count_ = count;
            rebuild_positions();
            needs_layout_ = true;
        }
    }
    
    // Set default estimated height for new rows
    void set_row_height(float height) {
        if (default_row_height_ != height) {
            default_row_height_ = height;
            rebuild_positions();
            needs_layout_ = true;
        }
    }
    
    void set_item_height(int index, float height) {
        if (index < 0 || index >= count_) return;
        
        float diff = height - (item_pos_[index + 1] - item_pos_[index]);
        if (std::abs(diff) < 0.1f) return;

        // Update this and all subsequent positions
        for (size_t i = index + 1; i < item_pos_.size(); ++i) {
            item_pos_[i] += diff;
        }
        
        needs_layout_ = true;
    }

    void set_create_row_fn(CreateRowFn fn) { create_row_ = std::move(fn); }
    void set_bind_row_fn(BindRowFn fn) { bind_row_ = std::move(fn); }

    int get_item_count() const { return count_; }

    void scroll_to(int index) {
        if (index < 0) index = 0;
        if (index >= count_) index = count_ - 1;
        
        float h = (last_h_ > 0) ? last_h_ : 100.0f; 
        float total_h = get_content_height();
        float max_scroll = std::max(0.0f, total_h - h);
        
        float target_y = item_pos_[index];
        scroll_y_ = std::clamp(target_y, 0.0f, max_scroll);
        needs_layout_ = true;
    }

    void refresh() {
        needs_layout_ = true;
    }

    // Widget Implementation
    const char* type_name() const override { return "VirtualizedListWidget"; }

    void update(float delta_ms, Element& elem) override { }

    void render(const Element& elem, Renderer& renderer) override {
        float w = elem.width();
        float h = elem.height();
        
        float content_h = get_content_height();
        
        // Draw Scrollbar 
        if (content_h > h && h > 0) {
            float bar_w = 6.0f;
            Color bar_bg{0.1f, 0.1f, 0.1f, 0.5f}; 
            Color thumb_c{0.4f, 0.4f, 0.4f, 0.8f}; 

            renderer.draw_rect(w - bar_w, 0, bar_w, h, 0, Paint::solid(bar_bg), Paint::none(), 0);

            float thumb_h = std::max(20.0f, h * h / content_h);
            float thumb_y = (content_h - h) > 0 ? (h - thumb_h) * scroll_y_ / (content_h - h) : 0;
            
            renderer.draw_rect(w - bar_w, thumb_y, bar_w, thumb_h, 3, Paint::solid(thumb_c), Paint::none(), 0);
        }

        // Logical Update
        if (h > 0) {
            if (w != last_w_ || h != last_h_ || needs_layout_) {
                last_w_ = w;
                last_h_ = h;
                needs_layout_ = false;
                const_cast<VirtualizedListWidget*>(this)->update_visible_rows(const_cast<Element&>(elem));
            } else {
                const_cast<VirtualizedListWidget*>(this)->sync_row_positions(const_cast<Element&>(elem));
            }
        }
    }

    bool handle_event(const Event& event, Element& elem) override {
        if (event.type == EventType::MouseWheel) {
            float h = elem.height();
            float content_h = get_content_height();
            float max_scroll = std::max(0.0f, content_h - h);
            float old_y = scroll_y_;
            
            scroll_y_ = std::clamp(scroll_y_ - event.delta_y, 0.0f, max_scroll);
            
            if (old_y != scroll_y_) {
                 update_visible_rows(elem);
                 elem.mark_paint_dirty();
            }
            return true;
        }

        if (event.type == EventType::MouseDown) {
             float lx = event.x - elem.absolute_x();
             if (lx > elem.width() - 15) { 
                  dragging_ = true;
                  drag_start_y_ = event.y;
                  drag_start_scroll_ = scroll_y_;
                  return true;
             }
        }
        
        if (event.type == EventType::MouseMove && dragging_) {
             float h = elem.height();
             float max_scroll = std::max(0.0f, get_content_height() - h);
             float dy = event.y - drag_start_y_;
             float scroll_ratio = (max_scroll > 0) ? max_scroll / h : 0;
             scroll_y_ = std::clamp(drag_start_scroll_ + dy * scroll_ratio, 0.0f, max_scroll);
             update_visible_rows(elem);
             elem.mark_paint_dirty();
             return true;
        }

        if (event.type == EventType::MouseUp) {
            dragging_ = false;
        }
        
        if (event.type == EventType::KeyDown) {
            float move = 0;
            if (event.key == KeyCode::Up) move = 20.0f;
            else if (event.key == KeyCode::Down) move = -20.0f;
            else if (event.key == KeyCode::PageUp) move = elem.height();
            else if (event.key == KeyCode::PageDown) move = -elem.height();
            else if (event.key == KeyCode::Home) {
                float old_y = scroll_y_;
                scroll_y_ = 0.0f;
                if (old_y != scroll_y_) {
                    update_visible_rows(elem);
                    elem.mark_paint_dirty();
                }
                return true;
            }
            else if (event.key == KeyCode::End) {
                float h = elem.height();
                float max_scroll = std::max(0.0f, get_content_height() - h);
                float old_y = scroll_y_;
                scroll_y_ = max_scroll;
                if (old_y != scroll_y_) {
                    update_visible_rows(elem);
                    elem.mark_paint_dirty();
                }
                return true;
            }
            
            if (move != 0) {
                float h = elem.height();
                float max_scroll = std::max(0.0f, get_content_height() - h);
                float old_y = scroll_y_;
                scroll_y_ = std::clamp(scroll_y_ - move, 0.0f, max_scroll);
                if (old_y != scroll_y_) {
                    update_visible_rows(elem);
                    elem.mark_paint_dirty();
                }
                return true;
            }
        }
        
        return false;
    }

    bool wants_mouse_capture() const override { return dragging_; }

private:
    int count_ = 0;
    float default_row_height_ = 40.0f;
    float scroll_y_ = 0.0f;
    float last_w_ = -1.0f;
    float last_h_ = -1.0f;
    bool needs_layout_ = true;

    CreateRowFn create_row_;
    BindRowFn bind_row_;
    
    bool dragging_ = false;
    float drag_start_y_ = 0;
    float drag_start_scroll_ = 0;

    std::vector<Element*> recycled_;
    
    // Stores Y position of each item + 1 (last one is total height)
    std::vector<float> item_pos_;

    struct ActiveRow {
        int index;
        Element* elem;
    };
    std::deque<ActiveRow> active_rows_;

    float get_content_height() const {
        return item_pos_.empty() ? 0.0f : item_pos_.back();
    }

    void rebuild_positions() {
        float old_total = get_content_height();
        
        std::vector<float> new_pos(count_ + 1);
        new_pos[0] = 0.0f;
        
        for (int i = 0; i < count_; ++i) {
            float h = default_row_height_;
            if (i < (int)item_pos_.size() - 1) {
                h = item_pos_[i+1] - item_pos_[i];
            }
            new_pos[i+1] = new_pos[i] + h;
        }
        
        item_pos_ = std::move(new_pos);
    }

    void sync_row_positions(Element& container) {
        float w = container.width();
        for (auto& row : active_rows_) {
             if (row.index >= (int)item_pos_.size() - 1) continue;

             float row_top = item_pos_[row.index];
             float row_height = item_pos_[row.index + 1] - row_top;
             float y_pos = row_top - scroll_y_;
             
             if (row.elem->computed_style) {
                 row.elem->computed_style->position = Position::Absolute;
                 row.elem->computed_style->top = y_pos;
                 row.elem->computed_style->left = 0;
                 row.elem->computed_style->width_is_percent = false;
                 row.elem->computed_style->width = w;
                 row.elem->computed_style->height = row_height;
             }
             
             row.elem->set_x(0);
             row.elem->set_y(y_pos);
             row.elem->set_layout_size(w, row_height);
             
             for (size_t i = 0; i < row.elem->child_count(); ++i) {
                 auto* child = row.elem->child_at(i);
                 if (child->layout_width() <= 0) {
                     child->set_layout_size(w, row_height);
                 }
             }
        }
    }

    void update_visible_rows(Element& container) {
        if (!create_row_ || !bind_row_ || item_pos_.empty()) return;
        
        float h = container.height();
        if (h <= 0) return;

        // Find visible range using binary search on positions
        auto it_start = std::upper_bound(item_pos_.begin(), item_pos_.end(), scroll_y_);
        int first_idx = (it_start == item_pos_.begin()) ? 0 : static_cast<int>(std::distance(item_pos_.begin(), it_start)) - 1;
        
        auto it_end = std::lower_bound(item_pos_.begin(), item_pos_.end(), scroll_y_ + h);
        int last_idx = static_cast<int>(std::distance(item_pos_.begin(), it_end));
        
        first_idx = std::clamp(first_idx, 0, count_ - 1);
        last_idx = std::clamp(last_idx, 0, count_ - 1);

        // 1. Recycle rows that are out of view
        auto it = active_rows_.begin();
        while (it != active_rows_.end()) {
            if (it->index < first_idx || it->index > last_idx) {
                container.remove(it->elem);
                recycled_.push_back(it->elem);
                it = active_rows_.erase(it);
            } else {
                ++it;
            }
        }

        // 2. Create/Reuse rows for new indices
        for (int i = first_idx; i <= last_idx; ++i) {
            bool found = false;
            for (auto& row : active_rows_) { if (row.index == i) { found = true; break; } }
            if (found) continue;

            Element* row_elem = nullptr;
            if (!recycled_.empty()) {
                row_elem = recycled_.back();
                recycled_.pop_back();
            } else {
                row_elem = create_row_();
            }
            
            if (row_elem) {
                if (row_elem->computed_style) {
                    row_elem->computed_style->position = Position::Absolute;
                }
                
                if (row_elem->parent() != &container) {
                    container.append(row_elem);
                }
                bind_row_(row_elem, i);
                active_rows_.push_back({i, row_elem});
            }
        }
        
        // 3. Sync positions
        sync_row_positions(container);
        
        container.mark_layout_dirty();
        container.mark_paint_dirty();
    }
};

} // namespace flexUI

#endif

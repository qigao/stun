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
            needs_layout_ = true;
        }
    }
    
    void set_row_height(float height) {
        if (row_height_ != height) {
            row_height_ = height;
            needs_layout_ = true;
        }
    }
    
    void set_create_row_fn(CreateRowFn fn) { create_row_ = std::move(fn); }
    void set_bind_row_fn(BindRowFn fn) { bind_row_ = std::move(fn); }

    int get_item_count() const { return count_; }

    void scroll_to(int index) {
        scroll_y_ = index * row_height_;
        needs_layout_ = true;
    }

    // Widget Implementation
    const char* type_name() const override { return "VirtualizedListWidget"; }

    void render(const Element& elem, Renderer& renderer) override {
        // Just draw scrollbar or debug info?
        // The children are rendered by the element system.
        // We can draw a scrollbar here.
        float w = elem.width();
        float h = elem.height();
        
        float content_h = count_ * row_height_;
        
        // Draw Scrollbar if needed
        if (content_h > h) {
            float bar_w = 8.0f;
            Color bar_bg{30, 30, 30}; // Dark track
            Color thumb_c{100, 100, 100}; // Light thumb

            // Track
            renderer.draw_rect(w - bar_w, 0, bar_w, h, 0, Paint::solid(bar_bg), Paint::none(), 0);

            // Thumb
            float thumb_h = std::max(20.0f, h * h / content_h);
            float thumb_y = (content_h - h) > 0 ? (h - thumb_h) * scroll_y_ / (content_h - h) : 0;
            
            renderer.draw_rect(w - bar_w, thumb_y, bar_w, thumb_h, 4, Paint::solid(thumb_c), Paint::none(), 0);

            // Store rects for hit testing
            track_rect_ = {w - bar_w, 0, bar_w, h};
            // thumb_y is mostly visual, logic handles scroll
        }

        // We also check layout update here if needed (e.g. if size changed)
        if (elem.width() != last_w_ || elem.height() != last_h_) {
            last_w_ = elem.width();
            last_h_ = elem.height();
            const_cast<VirtualizedListWidget*>(this)->update_visible_rows(const_cast<Element&>(elem));
        }
    }

    bool handle_event(const Event& event, Element& elem) override {
        bool repaint = false;

        if (event.type == EventType::MouseWheel) {
            float max_scroll = std::max(0.0f, count_ * row_height_ - elem.height());
            float old_y = scroll_y_;
            scroll_y_ = std::clamp(scroll_y_ - event.delta_y * row_height_, 0.0f, max_scroll);
            
            if (old_y != scroll_y_) {
                 update_visible_rows(elem);
                 repaint = true;
            }
            return true;
        }

        // Trivial dragging logic for scrollbar (Simplified)
        if (event.type == EventType::MouseDown) {
             float lx = event.x - elem.absolute_x();
             if (lx > elem.width() - 10) {
                 dragging_ = true;
                 drag_start_y_ = event.y;
                 drag_start_scroll_ = scroll_y_;
                 repaint = true;
                 return true;
             }
        }
        
        if (event.type == EventType::MouseMove && dragging_) {
             float max_scroll = std::max(0.0f, count_ * row_height_ - elem.height());
             float dy = event.y - drag_start_y_;
             // Map simple dy to scroll (approx)
             scroll_y_ = std::clamp(drag_start_scroll_ + dy * (max_scroll / elem.height()), 0.0f, max_scroll);
             update_visible_rows(elem);
             repaint = true;
             return true;
        }

        if (event.type == EventType::MouseUp) {
            dragging_ = false;
        }

        if (repaint) elem.mark_paint_dirty();
        
        return false;
    }

    bool wants_mouse_capture() const override { return dragging_; }

private:
    int count_ = 0;
    float row_height_ = 40.0f;
    float scroll_y_ = 0.0f;
    float last_w_ = 0;
    float last_h_ = 0;
    bool needs_layout_ = true;

    CreateRowFn create_row_;
    BindRowFn bind_row_;
    
    struct Rect { float x, y, w, h; };
    Rect track_rect_{};
    bool dragging_ = false;
    float drag_start_y_ = 0;
    float drag_start_scroll_ = 0;

    // Recycled elements (detached from DOM)
    std::vector<Element*> recycled_;
    
    // Active rows currently in DOM: index -> Element*
    // Usage: We map row_index to Element*. If an index is no longer visible, we recycle its element.
    // Actually, simply tracking active rows by index is enough.
    struct ActiveRow {
        int index;
        Element* elem;
    };
    std::deque<ActiveRow> active_rows_;

    void update_visible_rows(Element& container) {
        if (!create_row_ || !bind_row_) return;
        
        float h = container.height();
        if (h <= 0) return;

        int first_idx = static_cast<int>(std::floor(scroll_y_ / row_height_));
        int max_visible = static_cast<int>(std::ceil(h / row_height_)) + 1; // +1 buffer
        int last_idx = std::min(count_ - 1, first_idx + max_visible);
        
        // 1. Recycle rows that are out of view
        auto it = active_rows_.begin();
        while (it != active_rows_.end()) {
            if (it->index < first_idx || it->index > last_idx) {
                // Recycle
                container.remove(it->elem);
                
                // Hide it just in case, or reset state?
                // Just keep it in recycled list
                recycled_.push_back(it->elem);
                it = active_rows_.erase(it);
            } else {
                ++it;
            }
        }

        // 2. Create/Reuse rows for new indices
        // Efficient way: we need [first_idx, last_idx].
        // Check which ones are missing.
        
        // Since active_rows_ is usually contiguous, we can check front/back.
        // But simple map or linear scan for small N (N~20-50) is fine.
        
        auto find_row = [&](int idx) -> Element* {
            for (auto& row : active_rows_) {
                if (row.index == idx) return row.elem;
            }
            return nullptr;
        };

        for (int i = first_idx; i <= last_idx; ++i) {
            if (i < 0) continue;
            
            if (find_row(i)) continue; // Already active

            // Need new row
            Element* row_elem = nullptr;
            if (!recycled_.empty()) {
                row_elem = recycled_.back();
                recycled_.pop_back();
                container.append(row_elem);
            } else {
                row_elem = create_row_();
                if (row_elem) container.append(row_elem);
            }
            
            if (row_elem) {
                // Position absolute
                // Ensure the row has absolute position style
                if (row_elem->computed_style) {
                    row_elem->computed_style->position = Position::Absolute;
                    row_elem->computed_style->top = i * row_height_ - scroll_y_; 
                    row_elem->computed_style->left = 0;
                    row_elem->computed_style->right = 0; // Stretch width
                    row_elem->computed_style->height = row_height_;
                }
                
                // Bind data
                bind_row_(row_elem, i);
                
                active_rows_.push_back({i, row_elem});
            }
        }
        
        // 3. Update positions of ALL active rows (because scroll_y_ changed behavior relative to top 0?)
        // Wait, if we use `top: i * row_height_` and container has `scroll top`?
        // If container is `overflow: hidden`, does it scroll implicitly?
        // No, standard DOM assumption is that we translate the children OR we set `top` relative to container.
        // If we set `top = i * row_height_ - scroll_y_`, they move visually.
        
        for (auto& row : active_rows_) {
             if (row.elem->computed_style) {
                 row.elem->computed_style->top = row.index * row_height_ - scroll_y_;
             }
             row.elem->mark_layout_dirty();
        }
        
        container.mark_layout_dirty();
    }
};

} // namespace flexUI

#endif

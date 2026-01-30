/*
 * flexUI Designer - Design Canvas Implementation
 */

#include "flexui_designer/design_canvas.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace flexui_designer {

DesignCanvas::DesignCanvas() {
    width_ = 800;
    height_ = 600;
}

void DesignCanvas::render(flex::Renderer& renderer) {
    if (!visible_) return;

    float canvas_x = x_, canvas_y = y_;
    float canvas_w = width_, canvas_h = height_;
    
    if (show_rulers_) {
        canvas_x += RULER_SIZE;
        canvas_y += RULER_SIZE;
        canvas_w -= RULER_SIZE;
        canvas_h -= RULER_SIZE;
    }

    flex::Color canvas_bg_hex = flex::Color{0.08f, 0.08f, 0.1f, 1}; // Deep navy charcoal
    flex::Paint bg = flex::Paint::solid(canvas_bg_hex);
    renderer.draw_rect(canvas_x, canvas_y, canvas_w, canvas_h, 0, bg, flex::Paint::none(), 0);

    if (show_grid_) render_grid(renderer);
    if (show_rulers_) render_rulers(renderer);
    
    if (!widgets_) return;

    for (size_t i = 0; i < widgets_->size(); ++i) {
        bool selected = selected_id_ && (*widgets_)[i].id == *selected_id_;
        render_widget(renderer, (*widgets_)[i], selected);
        if (selected) render_handles(renderer, (*widgets_)[i]);
    }
    
    if (show_guides_) render_guides(renderer);
    if (box_selecting_) render_box_selection(renderer);
}

void DesignCanvas::render_grid(flex::Renderer& renderer) {
    float ox = show_rulers_ ? RULER_SIZE : 0;
    float oy = show_rulers_ ? RULER_SIZE : 0;
    flex::Paint grid_color = flex::Paint::solid(flex::Color{0.2f, 0.2f, 0.22f, 1});
    for (float gx = x_ + ox; gx < x_ + width_; gx += grid_size_)
        renderer.draw_rect(gx, y_ + oy, 1, height_ - oy, 0, grid_color, flex::Paint::none(), 0);
    for (float gy = y_ + oy; gy < y_ + height_; gy += grid_size_)
        renderer.draw_rect(x_ + ox, gy, width_ - ox, 1, 0, grid_color, flex::Paint::none(), 0);
}

void DesignCanvas::render_handles(flex::Renderer& renderer, const DesignWidget& w) {
    float ox = show_rulers_ ? RULER_SIZE : 0;
    float oy = show_rulers_ ? RULER_SIZE : 0;
    float wx = x_ + ox + w.x, wy = y_ + oy + w.y;
    float hs = HANDLE_SIZE;
    flex::Paint fill = flex::Paint::solid(flex::Color{1, 1, 1, 1});
    flex::Paint stroke = flex::Paint::solid(flex::Color{0.3f, 0.6f, 1.0f, 1});

    // 8 handles: corners + edges
    float cx = wx + w.width / 2, cy = wy + w.height / 2;
    float positions[][2] = {
        {wx - hs/2, wy - hs/2},                    // TopLeft
        {cx - hs/2, wy - hs/2},                    // Top
        {wx + w.width - hs/2, wy - hs/2},          // TopRight
        {wx + w.width - hs/2, cy - hs/2},          // Right
        {wx + w.width - hs/2, wy + w.height - hs/2}, // BottomRight
        {cx - hs/2, wy + w.height - hs/2},         // Bottom
        {wx - hs/2, wy + w.height - hs/2},         // BottomLeft
        {wx - hs/2, cy - hs/2}                     // Left
    };

    for (auto& pos : positions)
        renderer.draw_rect(pos[0], pos[1], hs, hs, 1, fill, stroke, 1);
}

void DesignCanvas::render_widget(flex::Renderer& renderer, const DesignWidget& w, bool selected) {
    float ox = show_rulers_ ? RULER_SIZE : 0;
    float oy = show_rulers_ ? RULER_SIZE : 0;
    float wx = x_ + ox + w.x, wy = y_ + oy + w.y;
    flex::Paint fill, stroke;
    flex::Color text_col{1, 1, 1, 1};

    renderer.save();
    renderer.translate(wx, wy);

    // Draw a very subtle shadow/glow for all widgets
    flex::Paint shadow = flex::Paint::solid(flex::Color{0, 0, 0, 0.15f});
    renderer.draw_rect(2, 2, w.width, w.height, 6, shadow, flex::Paint::none(), 0);

    switch (w.type) {
        case WidgetType::Button: {
            renderer.draw_rect(0, 0, w.width, w.height, 6, flex::Paint::solid(flex::Color{0.25f, 0.55f, 0.95f, 1}), flex::Paint::none(), 0);
            renderer.draw_rect(0, 0, w.width, w.height / 2, 6, flex::Paint::solid(flex::Color{1, 1, 1, 0.05f}), flex::Paint::none(), 0);
            renderer.draw_text(w.text, w.width/2 - w.text.length()*3.5f, (w.height - 13)/2, "sans", 13, true, text_col);
            break;
        }
        case WidgetType::Label:
            text_col = {0.95f, 0.95f, 0.98f, 1};
            renderer.draw_text(w.text, 0, 2, "sans", 13, false, text_col);
            break;
        case WidgetType::Input:
            fill = flex::Paint::solid(flex::Color{0.14f, 0.14f, 0.16f, 1});
            stroke = flex::Paint::solid(flex::Color{0.3f, 0.3f, 0.35f, 1});
            renderer.draw_rect(0, 0, w.width, w.height, 4, fill, stroke, 1);
            renderer.draw_text(w.text.empty() ? "Type here..." : w.text, 10, (w.height-12)/2, "sans", 12, false, {0.5f, 0.5f, 0.55f, 1});
            break;
        case WidgetType::Checkbox: {
            fill = flex::Paint::solid(flex::Color{0.14f, 0.14f, 0.16f, 1});
            stroke = flex::Paint::solid(flex::Color{0.3f, 0.3f, 0.35f, 1});
            float sz = std::min(w.height, 20.0f);
            renderer.draw_rect(0, (w.height-sz)/2, sz, sz, 4, fill, stroke, 1);
            if (w.checked) {
                float inner = sz * 0.6f;
                renderer.draw_rect((sz-inner)/2, (w.height-inner)/2, inner, inner, 2, flex::Paint::solid(flex::Color{0.25f, 0.6f, 1.0f, 1}), flex::Paint::none(), 0);
            }
            renderer.draw_text(w.text, sz + 8, (w.height-12)/2, "sans", 12, false, {0.9f, 0.9f, 0.95f, 1});
            break;
        }
        case WidgetType::Switch: {
            bool on = w.checked;
            flex::Color bg_color = on ? flex::Color{0.2f, 0.6f, 0.4f, 1} : flex::Color{0.25f, 0.25f, 0.28f, 1};
            float sw = 48, sh = 26;
            renderer.draw_rect(0, (w.height-sh)/2, sw, sh, sh/2, flex::Paint::solid(bg_color), flex::Paint::none(), 0);
            float knob_sz = sh - 4;
            float knob_x = on ? sw - knob_sz - 2 : 2;
            renderer.draw_rect(knob_x, (w.height-knob_sz)/2, knob_sz, knob_sz, knob_sz/2, flex::Paint::solid(flex::Color{1,1,1,1}), flex::Paint::none(), 0);
            break;
        }
        case WidgetType::Slider: {
            renderer.draw_rect(0, (w.height-6)/2, w.width, 6, 3, flex::Paint::solid(flex::Color{0.18f, 0.18f, 0.22f, 1}), flex::Paint::none(), 0);
            float pct = (w.value - w.min_value) / (w.max_value - w.min_value);
            renderer.draw_rect(0, (w.height-6)/2, w.width * pct, 6, 3, flex::Paint::solid(flex::Color{0.3f, 0.6f, 1.0f, 1}), flex::Paint::none(), 0);
            renderer.draw_rect(w.width * pct - 10, (w.height-20)/2, 20, 20, 10, flex::Paint::solid(flex::Color{1,1,1,1}), flex::Paint::solid(flex::Color{0.6f, 0.6f, 0.7f, 1}), 1);
            break;
        }
        case WidgetType::ProgressBar: {
            renderer.draw_rect(0, 0, w.width, w.height, w.height/2, flex::Paint::solid(flex::Color{0.14f, 0.14f, 0.16f, 1}), flex::Paint::none(), 0);
            float pct = (w.value - w.min_value) / (w.max_value - w.min_value);
            renderer.draw_rect(2, 2, (w.width - 4) * pct, w.height - 4, (w.height-4)/2, flex::Paint::solid(flex::Color{0.2f, 0.8f, 0.5f, 1}), flex::Paint::none(), 0);
            break;
        }
        case WidgetType::Dropdown:
            fill = flex::Paint::solid(flex::Color{0.14f, 0.14f, 0.16f, 1});
            stroke = flex::Paint::solid(flex::Color{0.3f, 0.3f, 0.35f, 1});
            renderer.draw_rect(0, 0, w.width, w.height, 5, fill, stroke, 1);
            renderer.draw_text(w.text, 12, (w.height-12)/2, "sans", 12, false, {0.95f, 0.95f, 0.95f, 1});
            renderer.draw_text("▼", w.width - 22, (w.height-10)/2, "sans", 10, false, {0.7f, 0.7f, 0.75f, 1});
            break;
        case WidgetType::Tabs: {
            renderer.draw_rect(0, 0, w.width, w.height, 6, flex::Paint::solid(flex::Color{0.12f, 0.12f, 0.14f, 1}), flex::Paint::none(), 0);
            float tab_w = (w.width - 4) / std::max(1, (int)w.options.size());
            for (size_t i = 0; i < w.options.size(); ++i) {
                bool active = (i == 0);
                if (active) {
                    renderer.draw_rect(2 + i * tab_w, 2, tab_w, w.height - 4, 4, flex::Paint::solid(flex::Color{0.25f, 0.35f, 0.55f, 1}), flex::Paint::none(), 0);
                }
                renderer.draw_text(w.options[i], 2 + i * tab_w + 12, (w.height-11)/2, "sans", 11, active, text_col);
            }
            break;
        }
        case WidgetType::Card: {
            renderer.draw_rect(0, 0, w.width, w.height, 12, flex::Paint::solid(flex::Color{0.15f, 0.15f, 0.18f, 1}), flex::Paint::solid(flex::Color{0.25f, 0.25f, 0.3f, 1}), 1);
            renderer.draw_rect(0, 0, w.width, 40, 12, flex::Paint::solid(flex::Color{1, 1, 1, 0.03f}), flex::Paint::none(), 0);
            renderer.draw_text(w.text, 20, 28, "sans", 15, true, text_col);
            break;
        }
        case WidgetType::Divider:
            renderer.draw_rect(0, w.height/2, w.width, 1, 0, flex::Paint::solid(flex::Color{0.25f, 0.25f, 0.3f, 1}), flex::Paint::none(), 0);
            break;
        case WidgetType::Container:
            renderer.draw_rect(0, 0, w.width, w.height, 8, flex::Paint::none(), flex::Paint::solid(flex::Color{0.3f, 0.35f, 0.45f, 0.6f}), 1.5f);
            break;
    }

    if (selected) {
        flex::Paint glow = flex::Paint::solid(flex::Color{0.3f, 0.6f, 1.0f, 0.2f});
        renderer.draw_rect(-5, -5, w.width + 10, w.height + 10, 10, glow, flex::Paint::none(), 0);
        stroke = flex::Paint::solid(flex::Color{0.3f, 0.6f, 1.0f, 1});
        renderer.draw_rect(-2, -2, w.width + 4, w.height + 4, 7, flex::Paint::none(), stroke, 2);
        if (inline_editing_) render_inline_edit(renderer, w);
    }
    
    if (w.locked) {
        flex::Paint lock_bg = flex::Paint::solid(flex::Color{0.8f, 0.4f, 0.2f, 0.9f});
        renderer.draw_rect(w.width - 16, 2, 14, 14, 3, lock_bg, flex::Paint::none(), 0);
        renderer.draw_text("L", w.width - 12, 13, "sans", 10, true, {1, 1, 1, 1});
    }
    
    if (!w.group_id.empty()) {
        flex::Paint group_bg = flex::Paint::solid(flex::Color{0.2f, 0.6f, 0.4f, 0.9f});
        renderer.draw_rect(2, 2, 14, 14, 3, group_bg, flex::Paint::none(), 0);
        renderer.draw_text("G", 6, 13, "sans", 10, true, {1, 1, 1, 1});
    }

    renderer.restore();
}

void DesignCanvas::render_inline_edit(flex::Renderer& renderer, const DesignWidget& w) {
    if (!inline_edit_buffer_) return;
    
    // Text edit position based on widget type - already relative to (wx, wy)
    float tx = 0, ty = 0, tw = w.width, th = 24;
    if (w.type == WidgetType::Button) { tx += 8; ty += (w.height - th) / 2; tw -= 16; }
    else if (w.type == WidgetType::Label) { ty -= 4; }
    else if (w.type == WidgetType::Input) { tx += 4; ty += (w.height - th) / 2; tw -= 8; }
    else if (w.type == WidgetType::Card) { tx += 12; ty += 8; tw -= 24; }
    
    // Edit background
    flex::Paint bg = flex::Paint::solid(flex::Color{0.1f, 0.1f, 0.12f, 0.95f});
    flex::Paint border = flex::Paint::solid(flex::Color{0.4f, 0.7f, 1.0f, 1});
    renderer.draw_rect(tx, ty, tw, th, 3, bg, border, 2);
    
    // Text
    renderer.draw_text(*inline_edit_buffer_, tx + 4, ty + 16, "sans", 12, false, {1, 1, 1, 1});
    
    // Cursor
    float cursor_x = tx + 4 + inline_edit_buffer_->length() * 6.5f;
    renderer.draw_rect(cursor_x, ty + 4, 2, th - 8, 0, flex::Paint::solid(flex::Color{1, 1, 1, 1}), flex::Paint::none(), 0);
}

ResizeHandle DesignCanvas::hit_test_handle(float x, float y) const {
    if (!widgets_ || !selected_id_) return ResizeHandle::None;

    for (const auto& w : *widgets_) {
        if (w.id != *selected_id_) continue;

        float wx = w.x, wy = w.y, hs = HANDLE_SIZE;
        float cx = wx + w.width / 2, cy = wy + w.height / 2;

        struct { float x, y; ResizeHandle h; } handles[] = {
            {wx, wy, ResizeHandle::TopLeft},
            {cx, wy, ResizeHandle::Top},
            {wx + w.width, wy, ResizeHandle::TopRight},
            {wx + w.width, cy, ResizeHandle::Right},
            {wx + w.width, wy + w.height, ResizeHandle::BottomRight},
            {cx, wy + w.height, ResizeHandle::Bottom},
            {wx, wy + w.height, ResizeHandle::BottomLeft},
            {wx, cy, ResizeHandle::Left}
        };

        for (auto& h : handles) {
            if (x >= h.x - hs && x <= h.x + hs && y >= h.y - hs && y <= h.y + hs)
                return h.h;
        }
        break;
    }
    return ResizeHandle::None;
}

bool DesignCanvas::handle_click(float x, float y) {
    float local_x = x - x_, local_y = y - y_;
    int idx = hit_test_widget(local_x, local_y);
    if (on_select_) on_select_(idx);
    return true;
}

int DesignCanvas::hit_test_widget(float x, float y) const {
    if (!widgets_) return -1;
    for (int i = (int)widgets_->size() - 1; i >= 0; --i) {
        const auto& w = (*widgets_)[i];
        if (x >= w.x && x < w.x + w.width && y >= w.y && y < w.y + w.height)
            return i;
    }
    return -1;
}

void DesignCanvas::start_widget_drag(int index, float offset_x, float offset_y) {
    widget_dragging_ = true;
    drag_widget_index_ = index;
    drag_offset_x_ = offset_x;
    drag_offset_y_ = offset_y;
}

void DesignCanvas::start_resize(int index, ResizeHandle handle, float x, float y) {
    if (index < 0 || !widgets_) return;
    resizing_ = true;
    resize_handle_ = handle;
    drag_widget_index_ = index;
    resize_start_x_ = x;
    resize_start_y_ = y;
    resize_orig_x_ = (*widgets_)[index].x;
    resize_orig_y_ = (*widgets_)[index].y;
    resize_orig_w_ = (*widgets_)[index].width;
    resize_orig_h_ = (*widgets_)[index].height;
}

void DesignCanvas::update_widget_drag(float x, float y) {
    if (!widget_dragging_ || drag_widget_index_ < 0) return;
    float new_x, new_y;
    if (snap_enabled_) {
        new_x = std::round((x - drag_offset_x_) / grid_size_) * grid_size_;
        new_y = std::round((y - drag_offset_y_) / grid_size_) * grid_size_;
    } else {
        new_x = x - drag_offset_x_;
        new_y = y - drag_offset_y_;
    }
    
    // Update guides for smart alignment
    if (widgets_ && show_guides_) {
        auto& w = (*widgets_)[drag_widget_index_];
        update_guides(new_x, new_y, w.width, w.height);
    }
    
    if (on_move_) on_move_(drag_widget_index_, new_x, new_y);
}

void DesignCanvas::update_resize(float x, float y) {
    if (!resizing_ || drag_widget_index_ < 0 || !widgets_) return;

    float dx = x - resize_start_x_, dy = y - resize_start_y_;
    float nx = resize_orig_x_, ny = resize_orig_y_;
    float nw = resize_orig_w_, nh = resize_orig_h_;

    switch (resize_handle_) {
        case ResizeHandle::TopLeft:     nx += dx; ny += dy; nw -= dx; nh -= dy; break;
        case ResizeHandle::Top:         ny += dy; nh -= dy; break;
        case ResizeHandle::TopRight:    ny += dy; nw += dx; nh -= dy; break;
        case ResizeHandle::Right:       nw += dx; break;
        case ResizeHandle::BottomRight: nw += dx; nh += dy; break;
        case ResizeHandle::Bottom:      nh += dy; break;
        case ResizeHandle::BottomLeft:  nx += dx; nw -= dx; nh += dy; break;
        case ResizeHandle::Left:        nx += dx; nw -= dx; break;
        default: break;
    }

    // Minimum size
    if (nw < 20) { nw = 20; nx = resize_orig_x_ + resize_orig_w_ - 20; }
    if (nh < 20) { nh = 20; ny = resize_orig_y_ + resize_orig_h_ - 20; }

    // Snap to grid
    if (snap_enabled_) {
        nx = std::round(nx / grid_size_) * grid_size_;
        ny = std::round(ny / grid_size_) * grid_size_;
        nw = std::round(nw / grid_size_) * grid_size_;
        nh = std::round(nh / grid_size_) * grid_size_;
    }

    if (on_resize_) on_resize_(drag_widget_index_, nx, ny, nw, nh);
}

void DesignCanvas::end_widget_drag() {
    widget_dragging_ = false;
    drag_widget_index_ = -1;
    guide_lines_h_.clear();
    guide_lines_v_.clear();
}

void DesignCanvas::end_resize() {
    resizing_ = false;
    resize_handle_ = ResizeHandle::None;
    drag_widget_index_ = -1;
}

// Box selection
void DesignCanvas::start_box_select(float x, float y) {
    box_selecting_ = true;
    box_start_x_ = box_end_x_ = x;
    box_start_y_ = box_end_y_ = y;
}

void DesignCanvas::update_box_select(float x, float y) {
    if (!box_selecting_) return;
    box_end_x_ = x;
    box_end_y_ = y;
}

void DesignCanvas::end_box_select() {
    if (!box_selecting_ || !widgets_) {
        box_selecting_ = false;
        return;
    }
    
    float min_x = std::min(box_start_x_, box_end_x_);
    float max_x = std::max(box_start_x_, box_end_x_);
    float min_y = std::min(box_start_y_, box_end_y_);
    float max_y = std::max(box_start_y_, box_end_y_);
    
    std::vector<int> selected;
    for (size_t i = 0; i < widgets_->size(); ++i) {
        const auto& w = (*widgets_)[i];
        // Widget intersects box
        if (w.x + w.width > min_x && w.x < max_x &&
            w.y + w.height > min_y && w.y < max_y) {
            selected.push_back((int)i);
        }
    }
    
    if (on_box_select_) on_box_select_(selected);
    box_selecting_ = false;
}

void DesignCanvas::render_box_selection(flex::Renderer& renderer) {
    float ox = show_rulers_ ? RULER_SIZE : 0;
    float oy = show_rulers_ ? RULER_SIZE : 0;
    float min_x = x_ + ox + std::min(box_start_x_, box_end_x_);
    float max_x = x_ + ox + std::max(box_start_x_, box_end_x_);
    float min_y = y_ + oy + std::min(box_start_y_, box_end_y_);
    float max_y = y_ + oy + std::max(box_start_y_, box_end_y_);
    
    flex::Paint fill = flex::Paint::solid(flex::Color{0.3f, 0.6f, 1.0f, 0.15f});
    flex::Paint stroke = flex::Paint::solid(flex::Color{0.3f, 0.6f, 1.0f, 0.8f});
    renderer.draw_rect(min_x, min_y, max_x - min_x, max_y - min_y, 0, fill, stroke, 1);
}

// Smart guides
void DesignCanvas::update_guides(float wx, float wy, float ww, float wh) {
    guide_lines_h_.clear();
    guide_lines_v_.clear();
    if (!widgets_) return;
    
    constexpr float SNAP_THRESHOLD = 5.0f;
    float wcx = wx + ww / 2, wcy = wy + wh / 2;
    float wr = wx + ww, wb = wy + wh;
    
    for (size_t i = 0; i < widgets_->size(); ++i) {
        if ((int)i == drag_widget_index_) continue;
        const auto& o = (*widgets_)[i];
        float ocx = o.x + o.width / 2, ocy = o.y + o.height / 2;
        float or_ = o.x + o.width, ob = o.y + o.height;
        
        // Vertical guides (x alignment)
        if (std::abs(wx - o.x) < SNAP_THRESHOLD) guide_lines_v_.push_back(o.x);
        if (std::abs(wr - or_) < SNAP_THRESHOLD) guide_lines_v_.push_back(or_);
        if (std::abs(wcx - ocx) < SNAP_THRESHOLD) guide_lines_v_.push_back(ocx);
        if (std::abs(wx - or_) < SNAP_THRESHOLD) guide_lines_v_.push_back(or_);
        if (std::abs(wr - o.x) < SNAP_THRESHOLD) guide_lines_v_.push_back(o.x);
        
        // Horizontal guides (y alignment)
        if (std::abs(wy - o.y) < SNAP_THRESHOLD) guide_lines_h_.push_back(o.y);
        if (std::abs(wb - ob) < SNAP_THRESHOLD) guide_lines_h_.push_back(ob);
        if (std::abs(wcy - ocy) < SNAP_THRESHOLD) guide_lines_h_.push_back(ocy);
        if (std::abs(wy - ob) < SNAP_THRESHOLD) guide_lines_h_.push_back(ob);
        if (std::abs(wb - o.y) < SNAP_THRESHOLD) guide_lines_h_.push_back(o.y);
    }
}

void DesignCanvas::render_guides(flex::Renderer& renderer) {
    float ox = show_rulers_ ? RULER_SIZE : 0;
    float oy = show_rulers_ ? RULER_SIZE : 0;
    flex::Paint guide_color = flex::Paint::solid(flex::Color{1.0f, 0.3f, 0.5f, 0.8f});
    
    for (float gx : guide_lines_v_) {
        renderer.draw_rect(x_ + ox + gx, y_ + oy, 1, height_ - oy, 0, guide_color, flex::Paint::none(), 0);
    }
    for (float gy : guide_lines_h_) {
        renderer.draw_rect(x_ + ox, y_ + oy + gy, width_ - ox, 1, 0, guide_color, flex::Paint::none(), 0);
    }
}

void DesignCanvas::render_rulers(flex::Renderer& renderer) {
    flex::Paint ruler_bg = flex::Paint::solid(flex::Color{0.16f, 0.16f, 0.18f, 1});
    flex::Paint tick_color = flex::Paint::solid(flex::Color{0.4f, 0.4f, 0.45f, 1});
    flex::Color text_col{0.5f, 0.5f, 0.55f, 1};
    
    // Top ruler
    renderer.draw_rect(x_ + RULER_SIZE, y_, width_ - RULER_SIZE, RULER_SIZE, 0, ruler_bg, flex::Paint::none(), 0);
    // Left ruler
    renderer.draw_rect(x_, y_ + RULER_SIZE, RULER_SIZE, height_ - RULER_SIZE, 0, ruler_bg, flex::Paint::none(), 0);
    // Corner
    renderer.draw_rect(x_, y_, RULER_SIZE, RULER_SIZE, 0, ruler_bg, flex::Paint::none(), 0);
    
    // Horizontal ticks (adjusted for zoom)
    float step = 10 * zoom_;
    if (step < 5) step = 50 * zoom_;
    for (float px = -std::fmod(pan_x_ * zoom_, step); px < width_ - RULER_SIZE; px += step) {
        float world_x = (px / zoom_) + pan_x_;
        float tick_h = (std::fmod(world_x, 100) < 0.1f) ? 10 : (std::fmod(world_x, 50) < 0.1f) ? 6 : 3;
        renderer.draw_rect(x_ + RULER_SIZE + px, y_ + RULER_SIZE - tick_h, 1, tick_h, 0, tick_color, flex::Paint::none(), 0);
        if (std::fmod(world_x, 100) < 0.1f && world_x >= 0) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", (int)world_x);
            renderer.draw_text(buf, x_ + RULER_SIZE + px + 2, y_ + 12, "sans", 9, false, text_col);
        }
    }
    
    // Vertical ticks (adjusted for zoom)
    for (float py = -std::fmod(pan_y_ * zoom_, step); py < height_ - RULER_SIZE; py += step) {
        float world_y = (py / zoom_) + pan_y_;
        float tick_w = (std::fmod(world_y, 100) < 0.1f) ? 10 : (std::fmod(world_y, 50) < 0.1f) ? 6 : 3;
        renderer.draw_rect(x_ + RULER_SIZE - tick_w, y_ + RULER_SIZE + py, tick_w, 1, 0, tick_color, flex::Paint::none(), 0);
        if (std::fmod(world_y, 100) < 0.1f && world_y >= 0) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", (int)world_y);
            renderer.draw_text(buf, x_ + 2, y_ + RULER_SIZE + py + 4, "sans", 9, false, text_col);
        }
    }
}

void DesignCanvas::screen_to_canvas(float sx, float sy, float& cx, float& cy) const {
    float ox = show_rulers_ ? RULER_SIZE : 0;
    float oy = show_rulers_ ? RULER_SIZE : 0;
    cx = (sx - x_ - ox) / zoom_ + pan_x_;
    cy = (sy - y_ - oy) / zoom_ + pan_y_;
}

void DesignCanvas::canvas_to_screen(float cx, float cy, float& sx, float& sy) const {
    float ox = show_rulers_ ? RULER_SIZE : 0;
    float oy = show_rulers_ ? RULER_SIZE : 0;
    sx = (cx - pan_x_) * zoom_ + x_ + ox;
    sy = (cy - pan_y_) * zoom_ + y_ + oy;
}

void DesignCanvas::zoom_fit() {
    if (!widgets_ || widgets_->empty()) {
        zoom_ = 1.0f;
        pan_x_ = pan_y_ = 0;
        return;
    }
    
    float min_x = 1e9f, min_y = 1e9f, max_x = -1e9f, max_y = -1e9f;
    for (const auto& w : *widgets_) {
        min_x = std::min(min_x, w.x);
        min_y = std::min(min_y, w.y);
        max_x = std::max(max_x, w.x + w.width);
        max_y = std::max(max_y, w.y + w.height);
    }
    
    float content_w = max_x - min_x + 40;
    float content_h = max_y - min_y + 40;
    float canvas_w = width_ - (show_rulers_ ? RULER_SIZE : 0);
    float canvas_h = height_ - (show_rulers_ ? RULER_SIZE : 0);
    
    zoom_ = std::min(canvas_w / content_w, canvas_h / content_h);
    zoom_ = std::max(0.25f, std::min(4.0f, zoom_));
    pan_x_ = min_x - 20;
    pan_y_ = min_y - 20;
}

void DesignCanvas::start_pan(float x, float y) {
    panning_ = true;
    pan_start_x_ = x;
    pan_start_y_ = y;
    pan_orig_x_ = pan_x_;
    pan_orig_y_ = pan_y_;
}

void DesignCanvas::update_pan(float x, float y) {
    if (!panning_) return;
    float dx = (x - pan_start_x_) / zoom_;
    float dy = (y - pan_start_y_) / zoom_;
    pan_x_ = pan_orig_x_ - dx;
    pan_y_ = pan_orig_y_ - dy;
}

void DesignCanvas::end_pan() {
    panning_ = false;
}

} // namespace flexui_designer

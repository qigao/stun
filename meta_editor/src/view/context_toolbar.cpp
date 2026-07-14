/*
 * Meta Editor - Floating Context Toolbar Implementation
 */

#include "meta_editor/view/context_toolbar.h"
#include "meta_editor/canvas.h"
#include "meta_editor/selection_manager.h"
#include <cmath>
#include <stb_sprintf.h>

namespace meta_editor {

ContextToolbar::ContextToolbar(Canvas* canvas, SelectionManager* selection)
    : canvas_(canvas), selection_(selection) {}

void ContextToolbar::update() {
    visible_ = selection_->has_selection();
    if (visible_) {
        update_from_selection();
        update_position();
    } else {
        fill_picker_open_ = false;
        stroke_picker_open_ = false;
    }
}

void ContextToolbar::update_from_selection() {
    // Get common fill
    auto fill = selection_->get_common_fill();
    if (fill && fill->type == flex::Paint::Type::Solid) {
        current_fill_ = fill->color;
    }

    // Get common stroke
    auto stroke = selection_->get_common_stroke();
    if (stroke && stroke->type == flex::Paint::Type::Solid) {
        current_stroke_ = stroke->color;
    }

    // Get common stroke width
    auto width = selection_->get_common_stroke_width();
    if (width) {
        current_stroke_width_ = *width;
    }

    // Get rough state from first selected shape
    auto* primary = selection_->primary_selection();
    if (primary && primary->type() == flex::NodeType::Shape) {
        auto* shape = static_cast<flex::Shape*>(primary);
        current_rough_ = shape->rough().roughness > 0;
    }
}

void ContextToolbar::update_position() {
    auto bounds = selection_->selection_bounds();
    auto bottom_center = canvas_->world_to_screen(
        bounds.x + bounds.width / 2,
        bounds.y + bounds.height
    );

    toolbar_x_ = bottom_center.x - toolbar_width_ / 2;
    toolbar_y_ = bottom_center.y + 16;  // 16px below selection

    // Clamp to viewport
    float vw = canvas_->width();
    float vh = canvas_->height();

    if (toolbar_x_ < 8) toolbar_x_ = 8;
    if (toolbar_x_ + toolbar_width_ > vw - 8) toolbar_x_ = vw - toolbar_width_ - 8;
    if (toolbar_y_ + toolbar_height_ > vh - 8) {
        // Place above selection instead
        auto top_center = canvas_->world_to_screen(
            bounds.x + bounds.width / 2,
            bounds.y
        );
        toolbar_y_ = top_center.y - toolbar_height_ - 16;
    }
}

void ContextToolbar::render(flex::Renderer& renderer) {
    if (!visible_) return;

    float x = toolbar_x_;
    float y = toolbar_y_;
    float h = toolbar_height_;
    float padding = 8;
    float swatch_size = 20;
    float gap = 8;

    // Background
    flex::Paint bg = flex::Paint::solid(flex::Color(0.17f, 0.17f, 0.17f, 0.95f));
    flex::Paint border = flex::Paint::solid(flex::Color(0.3f, 0.3f, 0.3f, 1.0f));
    renderer.draw_rect(x, y, toolbar_width_, h, 8, bg, border, 1.0f);

    float cx = x + padding;
    float cy = y + (h - swatch_size) / 2;

    // Fill swatch
    render_color_swatch(renderer, cx, cy, swatch_size, current_fill_, false);
    cx += swatch_size + gap;

    // Stroke swatch (with hole in middle to indicate stroke)
    render_color_swatch(renderer, cx, cy, swatch_size, current_stroke_, true);
    cx += swatch_size + gap;

    // Width indicator
    render_width_control(renderer, cx, cy);
    cx += 40 + gap;

    // Rough toggle
    render_rough_toggle(renderer, cx, cy);

    // Color picker popup
    if (fill_picker_open_) {
        render_color_picker(renderer, x + padding, y + h + 4, false);
    }
    if (stroke_picker_open_) {
        render_color_picker(renderer, x + padding + swatch_size + gap, y + h + 4, true);
    }
}

void ContextToolbar::render_color_swatch(flex::Renderer& r, float x, float y, float size,
                                          const flex::Color& color, bool is_stroke) {
    flex::Paint fill = flex::Paint::solid(color);
    flex::Paint outline = flex::Paint::solid(flex::Color(0.5f, 0.5f, 0.5f, 1.0f));

    if (is_stroke) {
        // Draw as ring to indicate stroke
        r.draw_rect(x, y, size, size, 4, fill, outline, 1.0f);
        // Cut out center
        flex::Paint bg = flex::Paint::solid(flex::Color(0.17f, 0.17f, 0.17f, 1.0f));
        r.draw_rect(x + 5, y + 5, size - 10, size - 10, 2, bg, flex::Paint::none(), 0);
    } else {
        r.draw_rect(x, y, size, size, 4, fill, outline, 1.0f);
    }
}

void ContextToolbar::render_color_picker(flex::Renderer& r, float x, float y, bool for_stroke) {
    float swatch = 24;
    float gap = 4;
    float cols = 4;
    float rows = 3;
    float pw = cols * swatch + (cols - 1) * gap + 16;
    float ph = rows * swatch + (rows - 1) * gap + 16;

    // Background
    flex::Paint bg = flex::Paint::solid(flex::Color(0.2f, 0.2f, 0.2f, 0.98f));
    flex::Paint border = flex::Paint::solid(flex::Color(0.4f, 0.4f, 0.4f, 1.0f));
    r.draw_rect(x, y, pw, ph, 6, bg, border, 1.0f);

    // Color grid
    for (int i = 0; i < 12; ++i) {
        int col = i % 4;
        int row = i / 4;
        float sx = x + 8 + col * (swatch + gap);
        float sy = y + 8 + row * (swatch + gap);

        flex::Paint fill = flex::Paint::solid(QUICK_COLORS[i]);
        flex::Paint outline = flex::Paint::solid(flex::Color(0.5f, 0.5f, 0.5f, 0.8f));
        r.draw_rect(sx, sy, swatch, swatch, 4, fill, outline, 1.0f);
    }
}

void ContextToolbar::render_width_control(flex::Renderer& r, float x, float y) {
    // Show stroke width as text
    char buf[16];
    stbsp_snprintf(buf, sizeof(buf), "%.0fpx", current_stroke_width_);

    flex::Paint text_color = flex::Paint::solid(flex::Color(0.8f, 0.8f, 0.8f, 1.0f));
    r.draw_text(buf, x, y + 15, "Arial", 12, false, text_color.color);
}

void ContextToolbar::render_rough_toggle(flex::Renderer& r, float x, float y) {
    float size = 20;
    flex::Paint bg = current_rough_
        ? flex::Paint::solid(flex::Color(0.4f, 0.4f, 0.9f, 1.0f))
        : flex::Paint::solid(flex::Color(0.3f, 0.3f, 0.3f, 1.0f));
    flex::Paint outline = flex::Paint::solid(flex::Color(0.5f, 0.5f, 0.5f, 1.0f));

    r.draw_rect(x, y, size, size, 4, bg, outline, 1.0f);

    // Draw squiggle icon
    flex::Paint icon = flex::Paint::solid(flex::Color(0.9f, 0.9f, 0.9f, 1.0f));
    std::string path = "M " + std::to_string(x + 4) + " " + std::to_string(y + 10) +
                       " Q " + std::to_string(x + 8) + " " + std::to_string(y + 6) +
                       " " + std::to_string(x + 10) + " " + std::to_string(y + 10) +
                       " Q " + std::to_string(x + 12) + " " + std::to_string(y + 14) +
                       " " + std::to_string(x + 16) + " " + std::to_string(y + 10);
    r.stroke_path(path, icon, 1.5f);
}

bool ContextToolbar::handle_click(float mx, float my) {
    if (!visible_) return false;

    float x = toolbar_x_;
    float y = toolbar_y_;
    float h = toolbar_height_;
    float padding = 8;
    float swatch_size = 20;
    float gap = 8;

    // Check if click is in toolbar area
    if (mx < x || mx > x + toolbar_width_ || my < y || my > y + h + 120) {
        fill_picker_open_ = false;
        stroke_picker_open_ = false;
        return false;
    }

    float cx = x + padding;
    float cy = y + (h - swatch_size) / 2;

    // Fill swatch click
    if (hit_test_swatch(mx, my, cx, cy, swatch_size)) {
        fill_picker_open_ = !fill_picker_open_;
        stroke_picker_open_ = false;
        return true;
    }
    cx += swatch_size + gap;

    // Stroke swatch click
    if (hit_test_swatch(mx, my, cx, cy, swatch_size)) {
        stroke_picker_open_ = !stroke_picker_open_;
        fill_picker_open_ = false;
        return true;
    }
    cx += swatch_size + gap;

    // Width control click (cycle through widths)
    if (mx >= cx && mx <= cx + 40 && my >= cy && my <= cy + swatch_size) {
        // Cycle: 1 -> 2 -> 4 -> 8 -> 1
        if (current_stroke_width_ < 2) current_stroke_width_ = 2;
        else if (current_stroke_width_ < 4) current_stroke_width_ = 4;
        else if (current_stroke_width_ < 8) current_stroke_width_ = 8;
        else current_stroke_width_ = 1;

        // Apply to selection
        auto stroke = selection_->get_common_stroke();
        if (stroke) {
            selection_->set_stroke(*stroke, current_stroke_width_);
        }
        return true;
    }
    cx += 40 + gap;

    // Rough toggle click
    if (hit_test_swatch(mx, my, cx, cy, swatch_size)) {
        current_rough_ = !current_rough_;
        // Apply to all selected shapes
        for (auto* node : selection_->selection()) {
            if (node->type() == flex::NodeType::Shape) {
                auto* shape = static_cast<flex::Shape*>(node);
                if (current_rough_) {
                    shape->set_rough(flex::RoughOptions::sketch());
                } else {
                    shape->set_rough(flex::RoughOptions::disabled());
                }
            }
        }
        return true;
    }

    // Color picker clicks
    if (fill_picker_open_) {
        int idx = hit_test_color_picker(mx, my, x + padding, y + h + 4);
        if (idx >= 0 && idx < 12) {
            current_fill_ = QUICK_COLORS[idx];
            selection_->set_fill(flex::Paint::solid(current_fill_));
            fill_picker_open_ = false;
            return true;
        }
    }

    if (stroke_picker_open_) {
        int idx = hit_test_color_picker(mx, my, x + padding + swatch_size + gap, y + h + 4);
        if (idx >= 0 && idx < 12) {
            current_stroke_ = QUICK_COLORS[idx];
            selection_->set_stroke(flex::Paint::solid(current_stroke_), current_stroke_width_);
            stroke_picker_open_ = false;
            return true;
        }
    }

    return false;
}

bool ContextToolbar::hit_test_swatch(float mx, float my, float sx, float sy, float size) {
    return mx >= sx && mx <= sx + size && my >= sy && my <= sy + size;
}

int ContextToolbar::hit_test_color_picker(float mx, float my, float px, float py) {
    float swatch = 24;
    float gap = 4;

    for (int i = 0; i < 12; ++i) {
        int col = i % 4;
        int row = i / 4;
        float sx = px + 8 + col * (swatch + gap);
        float sy = py + 8 + row * (swatch + gap);

        if (mx >= sx && mx <= sx + swatch && my >= sy && my <= sy + swatch) {
            return i;
        }
    }
    return -1;
}

} // namespace meta_editor

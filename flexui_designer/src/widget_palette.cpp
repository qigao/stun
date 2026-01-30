/*
 * flexUI Designer - Widget Palette Implementation
 */

#include "flexui_designer/widget_palette.h"

namespace flexui_designer {

WidgetPalette::WidgetPalette() {
    width_ = 200;
    height_ = 600;

    float y = 50;
    items_ = {
        {"Button", WidgetType::Button, y},
        {"Label", WidgetType::Label, y + 36},
        {"Input", WidgetType::Input, y + 72},
        {"Checkbox", WidgetType::Checkbox, y + 108},
        {"Switch", WidgetType::Switch, y + 144},
        {"Slider", WidgetType::Slider, y + 180},
        {"Progress", WidgetType::ProgressBar, y + 216},
        {"Dropdown", WidgetType::Dropdown, y + 252},
        {"Tabs", WidgetType::Tabs, y + 288},
        {"Card", WidgetType::Card, y + 324},
        {"Divider", WidgetType::Divider, y + 360},
        {"Container", WidgetType::Container, y + 396}
    };
}

void WidgetPalette::render(flex::Renderer& renderer) {
    if (!visible_) return;

    renderer.save();
    renderer.translate(x_, y_);

    // Panel background
    flex::Color palette_bg = flex::Color{0.1f, 0.1f, 0.12f, 0.98f};
    renderer.draw_rect(0, 0, width_, height_, 0, flex::Paint::solid(palette_bg), flex::Paint::none(), 0);
    // Vertical separator
    renderer.draw_rect(width_ - 1, 0, 1, height_, 0, flex::Paint::solid(flex::Color{0.25f, 0.25f, 0.3f, 1}), flex::Paint::none(), 0);

    renderer.draw_text("COMPONENTS", 16, 14, "sans", 11, true, flex::Color{0.5f, 0.7f, 1.0f, 1});

    flex::Color item_text{0.9f, 0.9f, 0.9f, 1};

    for (size_t i = 0; i < items_.size(); ++i) {
        auto& item = items_[i];
        float ix = 12;
        float iy = item.y;

        flex::Color bg_col = (hover_index_ == (int)i) 
            ? flex::Color{0.25f, 0.35f, 0.55f, 1} 
            : flex::Color{0.14f, 0.14f, 0.16f, 1};
        flex::Paint bg = flex::Paint::solid(bg_col);
        renderer.draw_rect(ix, iy, width_ - 24, 30, 6, bg, flex::Paint::none(), 0);
        renderer.draw_text(item.name, ix + 12, iy + 9, "sans", 12, false, item_text);
    }

    renderer.restore();
}

bool WidgetPalette::handle_click(float x, float y) {
    float local_y = y - y_;
    for (size_t i = 0; i < items_.size(); ++i) {
        if (local_y >= items_[i].y && local_y < items_[i].y + 30) {
            start_drag(items_[i].type, x, y);
            return true;
        }
    }
    return false;
}

void WidgetPalette::start_drag(WidgetType type, float x, float y) {
    dragging_ = true;
    drag_type_ = type;
    drag_x_ = x;
    drag_y_ = y;
}

void WidgetPalette::update_drag(float x, float y) {
    drag_x_ = x;
    drag_y_ = y;
}

void WidgetPalette::end_drag(float x, float y) {
    if (dragging_ && on_drop_) {
        on_drop_(drag_type_, x, y);
    }
    dragging_ = false;
}

void WidgetPalette::render_drag_preview(flex::Renderer& renderer) {
    if (!dragging_) return;

    flex::Paint fill = flex::Paint::solid(flex::Color{0.25f, 0.5f, 0.9f, 0.7f});
    flex::Paint stroke = flex::Paint::solid(flex::Color{0.4f, 0.7f, 1.0f, 1});
    
    float w = 100, h = 32;
    switch (drag_type_) {
        case WidgetType::Card: w = 200; h = 150; break;
        case WidgetType::Container: w = 250; h = 200; break;
        case WidgetType::Tabs: w = 300; h = 40; break;
        case WidgetType::Input:
        case WidgetType::Dropdown:
        case WidgetType::Slider:
        case WidgetType::ProgressBar: w = 150; break;
        default: break;
    }

    renderer.draw_rect(drag_x_ - w/2, drag_y_ - h/2, w, h, 4, fill, stroke, 2);

    const char* names[] = {"Button", "Label", "Input", "Checkbox", "Switch", 
                           "Slider", "Progress", "Dropdown", "Tabs", "Card", 
                           "Divider", "Container"};
    flex::Color white{1, 1, 1, 1};
    renderer.draw_text(names[(int)drag_type_], drag_x_ - 30, drag_y_ + 5, "sans", 12, false, white);
}

} // namespace flexui_designer

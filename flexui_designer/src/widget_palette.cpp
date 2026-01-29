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

    render_background(renderer);

    flex::Color white{1, 1, 1, 1};
    renderer.draw_text("Widgets", x_ + 16, y_ + 28, "sans", 14, true, white);

    flex::Color item_text{0.9f, 0.9f, 0.9f, 1};

    for (size_t i = 0; i < items_.size(); ++i) {
        auto& item = items_[i];
        float ix = x_ + 12;
        float iy = y_ + item.y;

        flex::Color bg_col = (hover_index_ == (int)i) 
            ? flex::Color{0.3f, 0.5f, 0.9f, 1} 
            : flex::Color{0.22f, 0.22f, 0.25f, 1};
        flex::Paint bg = flex::Paint::solid(bg_col);
        renderer.draw_rect(ix, iy, width_ - 24, 30, 4, bg, flex::Paint::none(), 0);
        renderer.draw_text(item.name, ix + 10, iy + 20, "sans", 12, false, item_text);
    }
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

/*
 * flexUI Designer - Widget Tree Implementation
 */

#include "flexui_designer/widget_tree.h"

namespace flexui_designer {

WidgetTree::WidgetTree() {
    width_ = 200;
    height_ = 300;
}

const char* WidgetTree::widget_type_name(WidgetType type) const {
    switch (type) {
        case WidgetType::Button: return "Button";
        case WidgetType::Label: return "Label";
        case WidgetType::Input: return "Input";
        case WidgetType::Checkbox: return "Checkbox";
        case WidgetType::Switch: return "Switch";
        case WidgetType::Slider: return "Slider";
        case WidgetType::ProgressBar: return "Progress";
        case WidgetType::Dropdown: return "Dropdown";
        case WidgetType::Tabs: return "Tabs";
        case WidgetType::Card: return "Card";
        case WidgetType::Divider: return "Divider";
        case WidgetType::Container: return "Container";
    }
    return "Widget";
}

const char* WidgetTree::widget_type_icon(WidgetType type) const {
    switch (type) {
        case WidgetType::Button: return "[B]";
        case WidgetType::Label: return "[T]";
        case WidgetType::Input: return "[I]";
        case WidgetType::Checkbox: return "[x]";
        case WidgetType::Switch: return "[~]";
        case WidgetType::Slider: return "[-]";
        case WidgetType::ProgressBar: return "[=]";
        case WidgetType::Dropdown: return "[v]";
        case WidgetType::Tabs: return "[|]";
        case WidgetType::Card: return "[#]";
        case WidgetType::Divider: return "[_]";
        case WidgetType::Container: return "[ ]";
    }
    return "[?]";
}

void WidgetTree::render(flex::Renderer& renderer) {
    if (!visible_) return;

    renderer.save();
    renderer.translate(x_, y_);

    // Background
    flex::Paint bg = flex::Paint::solid(flex::Color{0.14f, 0.14f, 0.16f, 1});
    flex::Paint border = flex::Paint::solid(flex::Color{0.25f, 0.25f, 0.28f, 1});
    renderer.draw_rect(0, 0, width_, height_, 0, bg, border, 1);

    // Title
    renderer.draw_text("Widgets", PADDING, 12, "sans", 12, true, {0.7f, 0.7f, 0.75f, 1});
    renderer.draw_rect(0, 26, width_, 1, 0, border, flex::Paint::none(), 0);

    if (!widgets_) {
        renderer.restore();
        return;
    }

    float item_y = 32 - scroll_y_;
    for (size_t i = 0; i < widgets_->size(); ++i) {
        if (item_y + ITEM_HEIGHT < 32) { item_y += ITEM_HEIGHT; continue; }
        if (item_y > height_) break;

        const auto& w = (*widgets_)[i];
        bool selected = selected_id_ && w.id == *selected_id_;

        // Selection highlight
        if (selected) {
            flex::Paint sel_bg = flex::Paint::solid(flex::Color{0.25f, 0.5f, 0.9f, 0.3f});
            renderer.draw_rect(2, item_y, width_ - 4, ITEM_HEIGHT - 2, 3, sel_bg, flex::Paint::none(), 0);
        }

        // Icon
        flex::Color icon_col = selected ? flex::Color{0.4f, 0.7f, 1.0f, 1} : flex::Color{0.5f, 0.5f, 0.55f, 1};
        renderer.draw_text(widget_type_icon(w.type), PADDING, item_y + 6, "sans", 10, false, icon_col);

        // Type name
        flex::Color text_col = selected ? flex::Color{1, 1, 1, 1} : flex::Color{0.8f, 0.8f, 0.85f, 1};
        renderer.draw_text(widget_type_name(w.type), PADDING + 28, item_y + 6, "sans", 11, false, text_col);

        // Widget text (truncated)
        if (!w.text.empty()) {
            std::string display = w.text.length() > 12 ? w.text.substr(0, 12) + ".." : w.text;
            renderer.draw_text(display, PADDING + 80, item_y + 6, "sans", 10, false, {0.5f, 0.5f, 0.55f, 1});
        }

        item_y += ITEM_HEIGHT;
    }

    renderer.restore();
}

bool WidgetTree::handle_click(float px, float py) {
    if (!widgets_ || !contains(px, py)) return false;

    float local_y = py - y_ - 32 + scroll_y_;
    int index = (int)(local_y / ITEM_HEIGHT);

    if (index >= 0 && index < (int)widgets_->size()) {
        if (on_select_) on_select_((*widgets_)[index].id);
        return true;
    }
    return false;
}

} // namespace flexui_designer

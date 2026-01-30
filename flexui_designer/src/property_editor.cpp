/*
 * flexUI Designer - Property Editor Implementation
 */

#include "flexui_designer/property_editor.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>

namespace flexui_designer {

enum FieldId { 
    F_ID = 1, F_X, F_Y, F_W, F_H, F_TEXT, F_VALUE, F_MIN, F_MAX,
    F_ON_CLICK, F_ON_CHANGE, F_ON_FOCUS, F_ON_BLUR,
    F_BG_COLOR, F_TEXT_COLOR, F_BORDER_COLOR,
    F_BORDER_WIDTH, F_BORDER_RADIUS, F_FONT_SIZE, F_PADDING
};

PropertyEditor::PropertyEditor() {
    width_ = 250;
    height_ = 600;
}

void PropertyEditor::set_widgets(std::vector<DesignWidget*> widgets) {
    widgets_batch_ = std::move(widgets);
    widget_ = widgets_batch_.empty() ? nullptr : widgets_batch_.front();
    editing_field_ = -1;
}

bool PropertyEditor::is_property_uniform(int field_id) const {
    if (widgets_batch_.size() <= 1) return true;
    
    auto* first = widgets_batch_.front();
    for (size_t i = 1; i < widgets_batch_.size(); ++i) {
        auto* w = widgets_batch_[i];
        switch (field_id) {
            case F_X: if (w->x != first->x) return false; break;
            case F_Y: if (w->y != first->y) return false; break;
            case F_W: if (w->width != first->width) return false; break;
            case F_H: if (w->height != first->height) return false; break;
            case F_TEXT: if (w->text != first->text) return false; break;
            case F_VALUE: if (w->value != first->value) return false; break;
            case F_MIN: if (w->min_value != first->min_value) return false; break;
            case F_MAX: if (w->max_value != first->max_value) return false; break;
            case F_ON_CLICK: if (w->on_click != first->on_click) return false; break;
            case F_ON_CHANGE: if (w->on_change != first->on_change) return false; break;
            case F_ON_FOCUS: if (w->on_focus != first->on_focus) return false; break;
            case F_ON_BLUR: if (w->on_blur != first->on_blur) return false; break;
            case F_BG_COLOR: if (w->bg_color != first->bg_color) return false; break;
            case F_TEXT_COLOR: if (w->text_color != first->text_color) return false; break;
            case F_BORDER_COLOR: if (w->border_color != first->border_color) return false; break;
            case F_BORDER_WIDTH: if (w->border_width != first->border_width) return false; break;
            case F_BORDER_RADIUS: if (w->border_radius != first->border_radius) return false; break;
            case F_FONT_SIZE: if (w->font_size != first->font_size) return false; break;
            case F_PADDING: if (w->padding != first->padding) return false; break;
        }
    }
    return true;
}

std::string PropertyEditor::get_batch_value(int field_id) const {
    if (widgets_batch_.empty()) return "";
    auto* w = widgets_batch_.front();
    char buf[64];
    switch (field_id) {
        case F_X: snprintf(buf, sizeof(buf), "%.0f", w->x); return buf;
        case F_Y: snprintf(buf, sizeof(buf), "%.0f", w->y); return buf;
        case F_W: snprintf(buf, sizeof(buf), "%.0f", w->width); return buf;
        case F_H: snprintf(buf, sizeof(buf), "%.0f", w->height); return buf;
        case F_TEXT: return w->text;
        case F_VALUE: snprintf(buf, sizeof(buf), "%.1f", w->value); return buf;
        case F_MIN: snprintf(buf, sizeof(buf), "%.1f", w->min_value); return buf;
        case F_MAX: snprintf(buf, sizeof(buf), "%.1f", w->max_value); return buf;
        case F_ON_CLICK: return w->on_click;
        case F_ON_CHANGE: return w->on_change;
        case F_ON_FOCUS: return w->on_focus;
        case F_ON_BLUR: return w->on_blur;
        case F_BORDER_WIDTH: snprintf(buf, sizeof(buf), "%.0f", w->border_width); return buf;
        case F_BORDER_RADIUS: snprintf(buf, sizeof(buf), "%.0f", w->border_radius); return buf;
        case F_FONT_SIZE: snprintf(buf, sizeof(buf), "%.0f", w->font_size); return buf;
        case F_PADDING: snprintf(buf, sizeof(buf), "%.0f", w->padding); return buf;
    }
    return "";
}

uint32_t PropertyEditor::get_batch_color(int field_id) const {
    if (widgets_batch_.empty()) return 0x2A2A2EFF;
    auto* w = widgets_batch_.front();
    switch (field_id) {
        case F_BG_COLOR: return w->bg_color;
        case F_TEXT_COLOR: return w->text_color;
        case F_BORDER_COLOR: return w->border_color;
    }
    return 0x2A2A2EFF;
}

bool PropertyEditor::all_widgets_have_type(WidgetType type) const {
    for (auto* w : widgets_batch_) {
        if (w->type != type) return false;
    }
    return !widgets_batch_.empty();
}

bool PropertyEditor::any_widget_has_type(WidgetType type) const {
    for (auto* w : widgets_batch_) {
        if (w->type == type) return true;
    }
    return false;
}

void PropertyEditor::render(flex::Renderer& renderer) {
    if (!visible_) return;

    renderer.save();
    renderer.translate(x_, y_);

    // Draw premium panel background (semi-transparent dark)
    flex::Color panel_bg = flex::Color{0.1f, 0.1f, 0.12f, 0.98f};
    renderer.draw_rect(0, 0, width_, height_, 0, flex::Paint::solid(panel_bg), flex::Paint::none(), 0);
    // Right border separator
    renderer.draw_rect(0, 0, 1, height_, 0, flex::Paint::solid(flex::Color{0.25f, 0.25f, 0.3f, 1}), flex::Paint::none(), 0);
    
    rows_.clear();

    flex::Color white{1, 1, 1, 1};
    
    // Title changes based on batch mode
    if (is_batch_mode()) {
        char title[64];
        snprintf(title, sizeof(title), "PROPERTIES (%zu objects)", widgets_batch_.size());
        renderer.draw_text(title, 16, 14, "sans", 11, true, flex::Color{0.5f, 0.7f, 1.0f, 1});
    } else {
        renderer.draw_text("PROPERTIES", 16, 14, "sans", 11, true, flex::Color{0.5f, 0.7f, 1.0f, 1});
    }

    if (widgets_batch_.empty()) {
        flex::Color hint{0.5f, 0.5f, 0.5f, 1};
        renderer.draw_text("Select a widget", 16, 50, "sans", 12, false, hint);
        renderer.restore();
        return;
    }

    float row_y = 50 - scroll_offset_;
    char buf[64];

    // In batch mode, show "Multiple" for type if types differ
    bool same_type = true;
    WidgetType first_type = widgets_batch_.front()->type;
    for (auto* w : widgets_batch_) {
        if (w->type != first_type) { same_type = false; break; }
    }

    const char* type_names[] = {
        "Button", "Label", "Input", "Checkbox", "Switch",
        "Slider", "ProgressBar", "Dropdown", "Tabs", "Card",
        "Divider", "Container"
    };

    // Widget section
    render_section_header(renderer, row_y, "Widget");
    row_y += 28;
    
    if (is_batch_mode()) {
        const char* type_display = same_type ? type_names[(int)first_type] : "Multiple";
        render_property_row(renderer, row_y, "Type", type_display, 0);
        row_y += 24;
        // ID not editable in batch mode
        snprintf(buf, sizeof(buf), "%zu widgets", widgets_batch_.size());
        render_property_row(renderer, row_y, "ID", buf, 0);
    } else {
        render_property_row(renderer, row_y, "Type", type_names[(int)widget_->type], 0);
        row_y += 24;
        render_property_row(renderer, row_y, "ID", widget_->id.c_str(), F_ID);
    }
    row_y += 32;

    // Position section - always shown
    render_section_header(renderer, row_y, "Position");
    row_y += 28;
    bool x_mixed = !is_property_uniform(F_X);
    bool y_mixed = !is_property_uniform(F_Y);
    render_property_row(renderer, row_y, "X", x_mixed ? "Mixed" : get_batch_value(F_X).c_str(), F_X, x_mixed);
    row_y += 24;
    render_property_row(renderer, row_y, "Y", y_mixed ? "Mixed" : get_batch_value(F_Y).c_str(), F_Y, y_mixed);
    row_y += 32;

    // Size section - always shown
    render_section_header(renderer, row_y, "Size");
    row_y += 28;
    bool w_mixed = !is_property_uniform(F_W);
    bool h_mixed = !is_property_uniform(F_H);
    render_property_row(renderer, row_y, "Width", w_mixed ? "Mixed" : get_batch_value(F_W).c_str(), F_W, w_mixed);
    row_y += 24;
    render_property_row(renderer, row_y, "Height", h_mixed ? "Mixed" : get_batch_value(F_H).c_str(), F_H, h_mixed);
    row_y += 32;

    // Content section - only if all widgets support text
    bool all_have_text = true;
    for (auto* w : widgets_batch_) {
        if (w->type != WidgetType::Button && w->type != WidgetType::Label &&
            w->type != WidgetType::Input && w->type != WidgetType::Checkbox &&
            w->type != WidgetType::Dropdown && w->type != WidgetType::Card) {
            all_have_text = false;
            break;
        }
    }
    if (all_have_text && !widgets_batch_.empty()) {
        render_section_header(renderer, row_y, "Content");
        row_y += 28;
        bool text_mixed = !is_property_uniform(F_TEXT);
        render_property_row(renderer, row_y, "Text", text_mixed ? "Mixed" : get_batch_value(F_TEXT).c_str(), F_TEXT, text_mixed);
        row_y += 32;
    }

    // Value section - only if all widgets are Slider or ProgressBar
    bool all_have_value = true;
    for (auto* w : widgets_batch_) {
        if (w->type != WidgetType::Slider && w->type != WidgetType::ProgressBar) {
            all_have_value = false;
            break;
        }
    }
    if (all_have_value && !widgets_batch_.empty()) {
        render_section_header(renderer, row_y, "Value");
        row_y += 28;
        bool val_mixed = !is_property_uniform(F_VALUE);
        bool min_mixed = !is_property_uniform(F_MIN);
        bool max_mixed = !is_property_uniform(F_MAX);
        render_property_row(renderer, row_y, "Value", val_mixed ? "Mixed" : get_batch_value(F_VALUE).c_str(), F_VALUE, val_mixed);
        row_y += 24;
        render_property_row(renderer, row_y, "Min", min_mixed ? "Mixed" : get_batch_value(F_MIN).c_str(), F_MIN, min_mixed);
        row_y += 24;
        render_property_row(renderer, row_y, "Max", max_mixed ? "Mixed" : get_batch_value(F_MAX).c_str(), F_MAX, max_mixed);
        row_y += 32;
    }

    // Events section - only show handlers common to all selected widgets
    bool show_events = false;
    bool show_on_click = all_widgets_have_type(WidgetType::Button);
    bool show_on_change = true;
    bool show_on_focus_blur = true;
    
    for (auto* w : widgets_batch_) {
        if (w->type != WidgetType::Input && w->type != WidgetType::Slider &&
            w->type != WidgetType::Checkbox && w->type != WidgetType::Switch &&
            w->type != WidgetType::Dropdown) {
            show_on_change = false;
        }
        if (w->type != WidgetType::Input) {
            show_on_focus_blur = false;
        }
    }
    
    if (show_on_click || show_on_change || show_on_focus_blur) {
        render_section_header(renderer, row_y, "Events");
        row_y += 28;
        
        if (show_on_click) {
            bool mixed = !is_property_uniform(F_ON_CLICK);
            render_property_row(renderer, row_y, "onClick", mixed ? "Mixed" : get_batch_value(F_ON_CLICK).c_str(), F_ON_CLICK, mixed);
            row_y += 24;
        }
        if (show_on_change) {
            bool mixed = !is_property_uniform(F_ON_CHANGE);
            render_property_row(renderer, row_y, "onChange", mixed ? "Mixed" : get_batch_value(F_ON_CHANGE).c_str(), F_ON_CHANGE, mixed);
            row_y += 24;
        }
        if (show_on_focus_blur) {
            bool focus_mixed = !is_property_uniform(F_ON_FOCUS);
            bool blur_mixed = !is_property_uniform(F_ON_BLUR);
            render_property_row(renderer, row_y, "onFocus", focus_mixed ? "Mixed" : get_batch_value(F_ON_FOCUS).c_str(), F_ON_FOCUS, focus_mixed);
            row_y += 24;
            render_property_row(renderer, row_y, "onBlur", blur_mixed ? "Mixed" : get_batch_value(F_ON_BLUR).c_str(), F_ON_BLUR, blur_mixed);
            row_y += 24;
        }
        row_y += 8;
    }

    // Style section - always shown (common to all widgets)
    render_section_header(renderer, row_y, "Style");
    row_y += 28;
    bool bg_mixed = !is_property_uniform(F_BG_COLOR);
    bool text_col_mixed = !is_property_uniform(F_TEXT_COLOR);
    bool border_col_mixed = !is_property_uniform(F_BORDER_COLOR);
    bool border_w_mixed = !is_property_uniform(F_BORDER_WIDTH);
    bool radius_mixed = !is_property_uniform(F_BORDER_RADIUS);
    bool font_mixed = !is_property_uniform(F_FONT_SIZE);
    bool padding_mixed = !is_property_uniform(F_PADDING);
    
    render_color_row(renderer, row_y, "Background", get_batch_color(F_BG_COLOR), F_BG_COLOR, bg_mixed);
    row_y += 24;
    render_color_row(renderer, row_y, "Text Color", get_batch_color(F_TEXT_COLOR), F_TEXT_COLOR, text_col_mixed);
    row_y += 24;
    render_color_row(renderer, row_y, "Border", get_batch_color(F_BORDER_COLOR), F_BORDER_COLOR, border_col_mixed);
    row_y += 24;
    render_property_row(renderer, row_y, "Border W", border_w_mixed ? "Mixed" : get_batch_value(F_BORDER_WIDTH).c_str(), F_BORDER_WIDTH, border_w_mixed);
    row_y += 24;
    render_property_row(renderer, row_y, "Radius", radius_mixed ? "Mixed" : get_batch_value(F_BORDER_RADIUS).c_str(), F_BORDER_RADIUS, radius_mixed);
    row_y += 24;
    render_property_row(renderer, row_y, "Font Size", font_mixed ? "Mixed" : get_batch_value(F_FONT_SIZE).c_str(), F_FONT_SIZE, font_mixed);
    row_y += 24;
    render_property_row(renderer, row_y, "Padding", padding_mixed ? "Mixed" : get_batch_value(F_PADDING).c_str(), F_PADDING, padding_mixed);

    renderer.restore();
}

void PropertyEditor::render_section_header(flex::Renderer& r, float y, const char* title) {
    if (y < 40 || y > height_) return;
    flex::Color color{0.4f, 0.4f, 0.45f, 1};
    r.draw_rect(12, y + 18, 40, 1, 0, flex::Paint::solid(flex::Color{0.25f, 0.25f, 0.3f, 1}), flex::Paint::none(), 0);
    r.draw_text(title, 12, y + 14, "sans", 10, true, color);
}

void PropertyEditor::render_property_row(flex::Renderer& r, float y, const char* label, const char* value, int field_id, bool is_mixed) {
    if (y < 40 || y > height_) return;
    
    flex::Color label_col{0.7f, 0.7f, 0.7f, 1};
    flex::Color value_col = is_mixed ? flex::Color{0.6f, 0.6f, 0.65f, 1} : flex::Color{0.9f, 0.9f, 0.9f, 1};
    
    r.draw_text(label, 16, y + 12, "sans", 11, false, label_col);

    float field_x = 80;
    float field_w = width_ - 96;
    
    bool editing = (editing_field_ == field_id && field_id > 0);
    
    if (field_id > 0) {
        flex::Color bg = editing ? flex::Color{0.18f, 0.18f, 0.22f, 1} : flex::Color{0.14f, 0.14f, 0.16f, 1};
        flex::Color border = editing ? flex::Color{0.3f, 0.6f, 1.0f, 1} : flex::Color{0.22f, 0.22f, 0.25f, 1};
        r.draw_rect(field_x, y - 2, field_w, 20, 4, flex::Paint::solid(bg), flex::Paint::solid(border), 1);
        rows_.push_back({label, y + y_, field_id, is_mixed});
    }

    const char* display = editing ? edit_buffer_.c_str() : value;
    r.draw_text(display, field_x + 6, y + 2, "sans", 11, is_mixed && !editing, value_col);
    
    if (editing) {
        float cursor_x = field_x + 6 + edit_buffer_.length() * 6.5f;
        r.draw_rect(cursor_x, y + 2, 1, 12, 0, flex::Paint::solid(flex::Color{1,1,1,1}), flex::Paint::none(), 0);
    }
}

void PropertyEditor::render_color_row(flex::Renderer& r, float y, const char* label, uint32_t color, int field_id, bool is_mixed) {
    if (y < 40 || y > height_) return;
    
    flex::Color label_col{0.7f, 0.7f, 0.7f, 1};
    r.draw_text(label, 16, y + 12, "sans", 11, false, label_col);

    float field_x = 80;
    float field_w = width_ - 96;
    
    bool editing = (editing_field_ == field_id);

    flex::Color border_color = editing ? flex::Color{0.4f, 0.6f, 1.0f, 1} : flex::Color{0.3f, 0.3f, 0.35f, 1};
    flex::Color bg_color = editing ? flex::Color{0.25f, 0.25f, 0.3f, 1} : flex::Color{0.2f, 0.2f, 0.22f, 1};
    r.draw_rect(field_x, y - 2, field_w, 20, 4, flex::Paint::solid(bg_color), flex::Paint::solid(border_color), 1);

    // Color preview
    float r_v, g_v, b_v, a_v;
    if (is_mixed) {
        r_v = g_v = b_v = 0.4f;
        a_v = 1.0f;
    } else {
        r_v = ((color >> 24) & 0xFF) / 255.0f;
        g_v = ((color >> 16) & 0xFF) / 255.0f;
        b_v = ((color >> 8) & 0xFF) / 255.0f;
        a_v = (color & 0xFF) / 255.0f;
    }
    
    r.draw_rect(field_x + 4, y, 16, 16, 3, flex::Paint::solid(flex::Color{r_v, g_v, b_v, a_v}), flex::Paint::none(), 0);
    
    char hex[16];
    const char* display;
    if (editing) {
        display = edit_buffer_.c_str();
    } else if (is_mixed) {
        display = "Mixed";
    } else {
        snprintf(hex, sizeof(hex), "#%02X%02X%02X%02X", 
            (int)(r_v*255), (int)(g_v*255), (int)(b_v*255), (int)(a_v*255));
        display = hex;
    }
    
    r.draw_text(display, field_x + 24, y + 2, "sans", 10, is_mixed && !editing, {0.85f, 0.85f, 0.85f, 1});

    if (editing) {
        float cursor_x = field_x + 24 + edit_buffer_.length() * 6.0f;
        r.draw_rect(cursor_x, y + 2, 1, 12, 0, flex::Paint::solid(flex::Color{1,1,1,1}), flex::Paint::none(), 0);
    }

    rows_.push_back({label, y + y_, field_id, is_mixed});
}

bool PropertyEditor::handle_click(float x, float y) {
    if (widgets_batch_.empty()) return false;

    float local_x = x - x_;
    float local_y = y - y_ + scroll_offset_;

    for (auto& row : rows_) {
        if (local_y >= row.y - 2 && local_y < row.y + 16 && local_x >= 80) {
            if (editing_field_ != row.field_id) {
                apply_edit();
                editing_field_ = row.field_id;
                
                // For batch mode, use first widget's value as starting point
                auto* w = widgets_batch_.front();
                switch (row.field_id) {
                    case F_ID: edit_buffer_ = w->id; break;
                    case F_X: edit_buffer_ = std::to_string((int)w->x); break;
                    case F_Y: edit_buffer_ = std::to_string((int)w->y); break;
                    case F_W: edit_buffer_ = std::to_string((int)w->width); break;
                    case F_H: edit_buffer_ = std::to_string((int)w->height); break;
                    case F_TEXT: edit_buffer_ = w->text; break;
                    case F_VALUE: edit_buffer_ = std::to_string((int)w->value); break;
                    case F_MIN: edit_buffer_ = std::to_string((int)w->min_value); break;
                    case F_MAX: edit_buffer_ = std::to_string((int)w->max_value); break;
                    case F_ON_CLICK: edit_buffer_ = w->on_click; break;
                    case F_ON_CHANGE: edit_buffer_ = w->on_change; break;
                    case F_ON_FOCUS: edit_buffer_ = w->on_focus; break;
                    case F_ON_BLUR: edit_buffer_ = w->on_blur; break;
                    case F_BG_COLOR: { char b[16]; snprintf(b, 16, "#%06X", w->bg_color >> 8); edit_buffer_ = b; } break;
                    case F_TEXT_COLOR: { char b[16]; snprintf(b, 16, "#%06X", w->text_color >> 8); edit_buffer_ = b; } break;
                    case F_BORDER_COLOR: { char b[16]; snprintf(b, 16, "#%06X", w->border_color >> 8); edit_buffer_ = b; } break;
                    case F_BORDER_WIDTH: edit_buffer_ = std::to_string((int)w->border_width); break;
                    case F_BORDER_RADIUS: edit_buffer_ = std::to_string((int)w->border_radius); break;
                    case F_FONT_SIZE: edit_buffer_ = std::to_string((int)w->font_size); break;
                    case F_PADDING: edit_buffer_ = std::to_string((int)w->padding); break;
                }
                
                // If mixed, clear buffer to let user type fresh value
                if (row.is_mixed) {
                    edit_buffer_.clear();
                }
            }
            return true;
        }
    }

    apply_edit();
    editing_field_ = -1;
    return false;
}

bool PropertyEditor::handle_text_input(const char* text) {
    if (editing_field_ <= 0) return false;
    edit_buffer_ += text;
    return true;
}

bool PropertyEditor::handle_key(int key) {
    if (editing_field_ <= 0) return false;

    if (key == '\r' || key == '\n') {
        apply_edit();
        editing_field_ = -1;
        return true;
    }
    if (key == 27) {
        editing_field_ = -1;
        return true;
    }
    if (key == '\b' || key == 127) {
        if (!edit_buffer_.empty()) edit_buffer_.pop_back();
        return true;
    }
    return false;
}

bool PropertyEditor::handle_scroll(float delta) {
    scroll_offset_ = std::max(0.0f, scroll_offset_ - delta * 30);
    return true;
}

static uint32_t parse_hex_color(const std::string& s) {
    if (s.empty()) return 0x2A2A2EFF;
    const char* p = s.c_str();
    if (*p == '#') p++;
    unsigned int val = 0;
    sscanf(p, "%x", &val);
    return (val << 8) | 0xFF;
}

void PropertyEditor::apply_edit() {
    if (editing_field_ <= 0 || widgets_batch_.empty()) return;

    // Apply edit to all widgets in batch
    for (auto* w : widgets_batch_) {
        switch (editing_field_) {
            case F_ID: 
                // ID only editable in single-select mode
                if (widgets_batch_.size() == 1) w->id = edit_buffer_; 
                break;
            case F_X: w->x = (float)std::atof(edit_buffer_.c_str()); break;
            case F_Y: w->y = (float)std::atof(edit_buffer_.c_str()); break;
            case F_W: w->width = std::max(20.0f, (float)std::atof(edit_buffer_.c_str())); break;
            case F_H: w->height = std::max(20.0f, (float)std::atof(edit_buffer_.c_str())); break;
            case F_TEXT: w->text = edit_buffer_; break;
            case F_VALUE: w->value = (float)std::atof(edit_buffer_.c_str()); break;
            case F_MIN: w->min_value = (float)std::atof(edit_buffer_.c_str()); break;
            case F_MAX: w->max_value = (float)std::atof(edit_buffer_.c_str()); break;
            case F_ON_CLICK: w->on_click = edit_buffer_; break;
            case F_ON_CHANGE: w->on_change = edit_buffer_; break;
            case F_ON_FOCUS: w->on_focus = edit_buffer_; break;
            case F_ON_BLUR: w->on_blur = edit_buffer_; break;
            case F_BG_COLOR: w->bg_color = parse_hex_color(edit_buffer_); break;
            case F_TEXT_COLOR: w->text_color = parse_hex_color(edit_buffer_); break;
            case F_BORDER_COLOR: w->border_color = parse_hex_color(edit_buffer_); break;
            case F_BORDER_WIDTH: w->border_width = std::max(0.0f, (float)std::atof(edit_buffer_.c_str())); break;
            case F_BORDER_RADIUS: w->border_radius = std::max(0.0f, (float)std::atof(edit_buffer_.c_str())); break;
            case F_FONT_SIZE: w->font_size = std::max(8.0f, (float)std::atof(edit_buffer_.c_str())); break;
            case F_PADDING: w->padding = std::max(0.0f, (float)std::atof(edit_buffer_.c_str())); break;
        }
    }

    // Single callback triggers single undo push in Designer
    if (on_change_) on_change_();
}

} // namespace flexui_designer

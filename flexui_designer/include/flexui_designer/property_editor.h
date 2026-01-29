/*
 * flexUI Designer - Property Editor
 *
 * Panel for editing selected widget properties.
 */

#pragma once

#include "designer.h"
#include <meta_editor/view/panel.h>
#include <functional>
#include <string>

namespace flexui_designer {

class PropertyEditor : public meta_editor::Panel {
public:
    PropertyEditor();

    void render(flex::Renderer& renderer) override;
    bool handle_click(float x, float y) override;
    bool handle_text_input(const char* text);
    bool handle_key(int key);
    bool handle_scroll(float delta);

    void set_widget(DesignWidget* widget) { 
        widget_ = widget; 
        widgets_batch_.clear();
        if (widget) widgets_batch_.push_back(widget);
        editing_field_ = -1; 
    }
    
    // Batch editing support
    void set_widgets(std::vector<DesignWidget*> widgets);
    bool is_batch_mode() const { return widgets_batch_.size() > 1; }
    const std::vector<DesignWidget*>& widgets_batch() const { return widgets_batch_; }

    using ChangeCallback = std::function<void()>;
    void set_change_callback(ChangeCallback cb) { on_change_ = std::move(cb); }

private:
    struct PropertyRow {
        const char* label;
        float y;
        int field_id;
        bool is_mixed;  // True if values differ across batch
    };

    void render_property_row(flex::Renderer& r, float y, const char* label, const char* value, int field_id, bool is_mixed = false);
    void render_color_row(flex::Renderer& r, float y, const char* label, uint32_t color, int field_id, bool is_mixed = false);
    void render_section_header(flex::Renderer& r, float y, const char* title);
    void apply_edit();
    
    // Batch editing helpers
    bool is_property_uniform(int field_id) const;
    std::string get_batch_value(int field_id) const;
    uint32_t get_batch_color(int field_id) const;
    bool all_widgets_have_type(WidgetType type) const;
    bool any_widget_has_type(WidgetType type) const;

    DesignWidget* widget_ = nullptr;
    std::vector<DesignWidget*> widgets_batch_;
    ChangeCallback on_change_;
    
    int editing_field_ = -1;
    std::string edit_buffer_;
    std::vector<PropertyRow> rows_;
    float scroll_offset_ = 0;
};

} // namespace flexui_designer

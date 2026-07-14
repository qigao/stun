/*
 * flexUI - LabelWidget
 *
 * Text label using Group/Shape composition system.
 */

#ifndef FLEXUI_LABEL_WIDGET_H
#define FLEXUI_LABEL_WIDGET_H

#include "../widget.h"
#include "../render_command.h"
#include <cstdint>
#include <string>
#include <vector>

namespace flexUI {

/**
 * LabelWidget - Text label using Group/Shape composition
 *
 * Structure:
 *   Group (root)
 *   └── TextShape (text)
 *
 * CSS variables:
 *   --text-overflow: "ellipsis" | "clip"
 *   --white-space: "normal" | "nowrap"
 *   --max-lines: "3"
 *   --text-align: "left" | "center" | "right"
 *   --vertical-align: "top" | "middle" | "bottom"
 */
class LabelWidget : public Widget {
public:
    explicit LabelWidget(const std::string& text = "");

    // Widget interface
    void emit_render_commands(const Element& elem, RenderCommandList& commands) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    bool needs_frame_update(const Element& elem) const override {
        (void)elem;
        return false;
    }
    bool state_affects_paint(Symbol state) const override {
        (void)state;
        return false;
    }
    const char* type_name() const override { return "LabelWidget"; }

    // Text access
    const std::string& text() const { return text_; }
    void set_text(const std::string& text);

private:
    void sync_host_semantics() override;
    bool render_cache_matches(const Element& elem,
                              const std::string& display_text) const;
    void update_render_cache_key(const Element& elem,
                                 const std::string& display_text);
    void invalidate_render_cache();

    std::string text_;
    bool render_cache_valid_ = false;
    std::string cached_text_;
    float cached_width_ = 0.0f;
    float cached_height_ = 0.0f;
    float cached_font_size_ = 0.0f;
    float cached_letter_spacing_ = 0.0f;
    float cached_word_spacing_ = 0.0f;
    float cached_text_indent_ = 0.0f;
    float cached_tab_size_ = 8.0f;
    int cached_font_weight_ = 0;
    int cached_font_style_ = 0;
    int cached_text_align_ = 0;
    int cached_text_transform_ = 0;
    int cached_direction_ = 0;
    uint64_t cached_style_signature_ = 0;
    std::string cached_font_family_;
    Color cached_text_color_;
    std::string cached_white_space_;
    std::string cached_overflow_wrap_;
    std::string cached_word_break_;
    std::string cached_text_overflow_;
    std::string cached_max_lines_;
    std::string cached_line_clamp_;
    std::string cached_vertical_align_;
    std::string cached_line_height_;
    std::string cached_text_decoration_;
    std::string cached_text_decoration_thickness_;
    std::string cached_text_underline_offset_;
    std::string cached_font_variant_numeric_;
    std::vector<RenderCommand> render_cache_;
};

} // namespace flexUI

#endif // FLEXUI_LABEL_WIDGET_H

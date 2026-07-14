/*
 * flexUI - ButtonWidget
 *
 * Button using Group/Shape composition system.
 */

#ifndef FLEXUI_BUTTON_WIDGET_H
#define FLEXUI_BUTTON_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../render_command.h"
#include "../shapes.h"
#include <cstdint>
#include <string>
#include <vector>

namespace flexUI {

/**
 * ButtonWidget - Button using Group/Shape composition
 *
 * Structure:
 *   Group (root)
 *   ├── RectShape (background)
 *   ├── CircleShape[] (ripples, dynamic)
 *   ├── TextShape (label) or CircleShape (spinner)
 *
 * CSS variables:
 *   --ripple: "true" | "false"
 *   --ripple-color: "r,g,b,a"
 *   --transition-duration: "300"
 *   --hover-scale: "1.05"
 *   --active-scale: "0.95"
 *   --loading-spinner-color: "r,g,b,a"
 */
class ButtonWidget : public Widget {
public:
    enum class Variant {
        Default,
        Secondary,
        Outline,
        Ghost,
        Destructive,
    };

    enum class Size {
        Small,
        Default,
        Large,
        Icon,
    };

    explicit ButtonWidget(const std::string& text = "");

    // Widget interface
    void emit_render_commands(const Element& elem, RenderCommandList& commands) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    bool needs_frame_update(const Element& elem) const override;
    bool measure_intrinsic_size(const Element& elem, float available_width,
                                float available_height, float& out_width,
                                float& out_height) const override;
    const char* type_name() const override { return "ButtonWidget"; }
    bool paints_host_box() const override { return true; }
    bool paints_part_box(std::string_view part_name) const override;
    bool emit_part_render_commands(const Element& host, const Element& part,
                                   std::string_view part_name,
                                   RenderCommandList& commands) override;
    void sync_host_semantics_for_layout(Element& elem) override;

    Element* label_element() { return label_element_; }
    Element* ripple_layer_element() { return ripple_layer_; }
    Element* spinner_element() { return spinner_element_; }
    const Element* label_element() const { return label_element_; }
    const Element* ripple_layer_element() const { return ripple_layer_; }
    const Element* spinner_element() const { return spinner_element_; }

    // Text access
    const std::string& text() const { return text_; }
    void set_text(const std::string& text) { text_ = text; sync_host_semantics(); invalidate_render_cache(); }

    // State control
    bool is_disabled() const { return disabled_; }
    void set_disabled(bool disabled) { disabled_ = disabled; sync_host_semantics(); invalidate_render_cache(); }

    bool is_loading() const { return loading_; }
    void set_loading(bool loading) { loading_ = loading; sync_host_semantics(); invalidate_render_cache(); }

    Variant variant() const { return variant_; }
    void set_variant(Variant variant) { variant_ = variant; sync_host_semantics(); invalidate_render_cache(); }

    Size size() const { return size_; }
    void set_size(Size size) { size_ = size; sync_host_semantics(); invalidate_render_cache(); }

private:
    void build_semantic_tree() override;
    void sync_host_semantics() override;
    void update_part_geometry(const Element& elem);
    Color resolved_text_color(const Element& elem) const;
    void rebuild_shapes(const Element& elem);
    void update_shapes(const Element& elem);
    void render_ripples(RenderCommandList& commands, const Transform& local_transform, float opacity, const Element& elem);
    bool render_cache_matches(const Element& elem,
                              const flex::RendererCapabilities& caps) const;
    void update_render_cache_key(const Element& elem,
                                 const flex::RendererCapabilities& caps);
    void invalidate_render_cache();

    // Ripple effect
    struct Ripple {
        float x, y;
        float radius;
        float max_radius;
        float alpha;
        bool finished = false;
    };

    void add_ripple(float x, float y, const Element& elem);
    void update_ripples(float delta_ms, Element& elem);

    // Visual composition
    Group root_;
    RectShape* background_ = nullptr;
    std::vector<TextShape*> text_shapes_;
    std::vector<LineShape*> text_decoration_shapes_;
    PathShape* spinner_ = nullptr;

    Element* label_element_ = nullptr;
    Element* ripple_layer_ = nullptr;
    Element* spinner_element_ = nullptr;

    // State
    std::string text_;
    bool disabled_ = false;
    bool loading_ = false;
    Variant variant_ = Variant::Default;
    Size size_ = Size::Default;

    // Ripples
    std::vector<Ripple> ripples_;

    // Animation
    float current_scale_ = 1.0f;
    float target_scale_ = 1.0f;
    float spinner_rotation_ = 0.0f;

    // Cached dimensions
    float cached_width_ = 0;
    float cached_height_ = 0;

    bool render_cache_valid_ = false;
    bool cached_scaling_capability_ = false;
    bool cached_hover_state_ = false;
    bool cached_active_state_ = false;
    bool cached_focus_state_ = false;
    bool cached_disabled_state_ = false;
    bool cached_loading_state_ = false;
    uint64_t cached_style_signature_ = 0;
    int cached_variant_ = 0;
    int cached_size_ = 0;
    float cached_render_width_ = 0.0f;
    float cached_render_height_ = 0.0f;
    float cached_render_scale_ = 1.0f;
    float cached_opacity_ = 1.0f;
    float cached_font_size_ = 0.0f;
    float cached_letter_spacing_ = 0.0f;
    float cached_word_spacing_ = 0.0f;
    float cached_text_indent_ = 0.0f;
    float cached_tab_size_ = 8.0f;
    float cached_border_radius_ = 0.0f;
    Color cached_background_color_;
    Color cached_text_color_;
    Color cached_border_color_;
    int cached_font_weight_ = 0;
    int cached_font_style_ = 0;
    int cached_text_align_ = 0;
    int cached_text_transform_ = 0;
    int cached_direction_ = 0;
    std::string cached_text_;
    std::string cached_font_family_;
    std::string cached_button_radius_;
    std::string cached_button_font_size_;
    std::string cached_button_bg_;
    std::string cached_bg_;
    std::string cached_button_bg_hover_;
    std::string cached_button_bg_active_;
    std::string cached_button_text_;
    std::string cached_text_var_;
    std::string cached_button_border_;
    std::string cached_border_color_var_;
    std::string cached_border_var_;
    std::string cached_button_border_focus_;
    std::string cached_button_border_width_;
    std::string cached_text_decoration_;
    std::string cached_text_decoration_color_;
    std::string cached_text_decoration_thickness_;
    std::string cached_text_underline_offset_;
    std::string cached_font_variant_numeric_;
    std::string cached_white_space_;
    std::string cached_overflow_wrap_;
    std::string cached_word_break_;
    std::string cached_text_overflow_;
    std::string cached_max_lines_;
    std::string cached_line_clamp_;
    std::string cached_line_height_;
    std::string cached_ripple_;
    std::vector<RenderCommand> render_cache_;
};

} // namespace flexUI

#endif // FLEXUI_BUTTON_WIDGET_H

/*
 * tvgbox2 - ButtonWidget
 *
 * Button using Group/Shape composition system.
 */

#ifndef TVGBOX2_BUTTON_WIDGET_H
#define TVGBOX2_BUTTON_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <string>
#include <vector>

namespace tvgbox2 {

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
    explicit ButtonWidget(const std::string& text = "");

    // Widget interface
    void render(const Element& elem, Renderer& renderer) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    const char* type_name() const override { return "ButtonWidget"; }

    // Text access
    const std::string& text() const { return text_; }
    void set_text(const std::string& text) { text_ = text; dirty_ = true; }

    // State control
    bool is_disabled() const { return disabled_; }
    void set_disabled(bool disabled) { disabled_ = disabled; dirty_ = true; }

    bool is_loading() const { return loading_; }
    void set_loading(bool loading) { loading_ = loading; dirty_ = true; }

private:
    void rebuild_shapes(const Element& elem);
    void update_shapes(const Element& elem);
    void render_ripples(flex::Renderer& renderer, const Transform& world_transform, float opacity, const Element& elem);

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
    TextShape* text_shape_ = nullptr;
    PathShape* spinner_ = nullptr;

    // State
    std::string text_;
    bool disabled_ = false;
    bool loading_ = false;

    // Ripples
    std::vector<Ripple> ripples_;

    // Animation
    float current_scale_ = 1.0f;
    float target_scale_ = 1.0f;
    float spinner_rotation_ = 0.0f;

    // Cached dimensions
    float cached_width_ = 0;
    float cached_height_ = 0;
};

} // namespace tvgbox2

#endif // TVGBOX2_BUTTON_WIDGET_H

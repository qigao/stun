/*
 * flexUI - LabelWidget
 *
 * Text label using Group/Shape composition system.
 */

#ifndef FLEXUI_LABEL_WIDGET_H
#define FLEXUI_LABEL_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <string>

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
    void render(const Element& elem, Renderer& renderer) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    const char* type_name() const override { return "LabelWidget"; }

    // Text access
    const std::string& text() const { return text_; }
    void set_text(const std::string& text);

private:
    void rebuild_shapes(const Element& elem);
    std::string process_text(const std::string& text, const Element& elem);
    std::string apply_ellipsis(const std::string& text, float max_width, float font_size);

    // Visual composition
    Group root_;
    TextShape* text_shape_ = nullptr;

    // State
    std::string text_;
    std::string cached_display_text_;
    float cached_width_ = 0;
    float cached_height_ = 0;
};

} // namespace flexUI

#endif // FLEXUI_LABEL_WIDGET_H

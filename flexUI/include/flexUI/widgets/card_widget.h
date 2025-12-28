/*
 * flexUI - CardWidget
 *
 * Card container using Group/Shape composition system.
 */

#ifndef FLEXUI_CARD_WIDGET_H
#define FLEXUI_CARD_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <string>

namespace flexUI {

/**
 * CardWidget - Container card with header, content, footer areas
 *
 * Structure:
 *   Group (root)
 *   ├── RectShape (card background with shadow)
 *   ├── RectShape (header bg, optional)
 *   ├── TextShape (title)
 *   ├── TextShape (subtitle)
 *   └── RectShape (footer bg, optional)
 *
 * CSS variables:
 *   --card-bg: "r,g,b,a"
 *   --card-border: "r,g,b,a"
 *   --card-border-width: "1"
 *   --card-radius: "8"
 *   --card-shadow: "0,0,0,0.1" (shadow color)
 *   --card-shadow-blur: "10"
 *   --card-padding: "16"
 *   --header-bg: "r,g,b,a" (optional)
 *   --footer-bg: "r,g,b,a" (optional)
 */
class CardWidget : public Widget {
public:
    CardWidget();

    // Widget interface
    void render(const Element& elem, Renderer& renderer) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    const char* type_name() const override { return "CardWidget"; }

    // Properties
    const std::string& title() const { return title_; }
    void set_title(const std::string& title) { title_ = title; dirty_ = true; }

    const std::string& subtitle() const { return subtitle_; }
    void set_subtitle(const std::string& subtitle) { subtitle_ = subtitle; dirty_ = true; }

    bool has_header() const { return has_header_; }
    void set_has_header(bool has) { has_header_ = has; dirty_ = true; }

    bool has_footer() const { return has_footer_; }
    void set_has_footer(bool has) { has_footer_ = has; dirty_ = true; }

    float header_height() const { return header_height_; }
    void set_header_height(float h) { header_height_ = h; dirty_ = true; }

    float footer_height() const { return footer_height_; }
    void set_footer_height(float h) { footer_height_ = h; dirty_ = true; }

private:
    void rebuild_shapes(const Element& elem);
    void update_shapes(const Element& elem);

    // Visual composition
    Group root_;
    RectShape* background_ = nullptr;
    RectShape* header_bg_ = nullptr;
    RectShape* footer_bg_ = nullptr;
    TextShape* title_text_ = nullptr;
    TextShape* subtitle_text_ = nullptr;

    // State
    std::string title_;
    std::string subtitle_;
    bool has_header_ = false;
    bool has_footer_ = false;
    float header_height_ = 60.0f;
    float footer_height_ = 50.0f;

    // Cached
    float cached_width_ = 0;
    float cached_height_ = 0;
};

} // namespace flexUI

#endif // FLEXUI_CARD_WIDGET_H

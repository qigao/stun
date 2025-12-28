/*
 * flexUI - CardWidget Implementation
 */

#include <flexUI/widgets/card_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/renderer.h>

namespace flexUI {

CardWidget::CardWidget() {}

void CardWidget::rebuild_shapes(const Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    if (elem.width() == cached_width_ &&
        elem.height() == cached_height_ &&
        background_ != nullptr) {
        return;
    }

    root_.clear();
    cached_width_ = elem.width();
    cached_height_ = elem.height();

    float radius = style->get_variable_float("--card-radius", 8.0f);
    float padding = style->get_variable_float("--card-padding", 16.0f);

    // Main background
    background_ = root_.add<RectShape>(0, 0, elem.width(), elem.height(), radius);

    // Header background
    if (has_header_) {
        header_bg_ = root_.add<RectShape>(0, 0, elem.width(), header_height_, radius);
    }

    // Footer background
    if (has_footer_) {
        float footer_y = elem.height() - footer_height_;
        footer_bg_ = root_.add<RectShape>(0, footer_y, elem.width(), footer_height_, radius);
    }

    // Title text
    if (!title_.empty()) {
        float title_y = has_header_ ? padding : padding;
        title_text_ = root_.add<TextShape>(padding, title_y, title_);
        title_text_->set_font_family(style->font_family);
        title_text_->set_font_size(style->font_size * 1.2f);
    }

    // Subtitle text
    if (!subtitle_.empty() && title_text_) {
        float subtitle_y = (has_header_ ? padding : padding) + style->font_size * 1.4f;
        subtitle_text_ = root_.add<TextShape>(padding, subtitle_y, subtitle_);
        subtitle_text_->set_font_family(style->font_family);
        subtitle_text_->set_font_size(style->font_size * 0.9f);
    }
}

void CardWidget::update_shapes(const Element& elem) {
    auto* style = elem.computed_style;
    if (!style || !background_) return;

    Color bg_color = style->get_variable_color("--card-bg", color_from_u8(255, 255, 255, 255));
    Color border_color = style->get_variable_color("--card-border", color_from_u8(229, 231, 235, 255));
    float border_width = style->get_variable_float("--card-border-width", 1.0f);

    background_->set_fill(bg_color);
    if (border_width > 0) {
        background_->set_stroke(border_color, border_width);
    }

    // Header styling
    if (header_bg_) {
        Color header_color = style->get_variable_color("--header-bg", color_from_u8(249, 250, 251, 255));
        header_bg_->set_fill(header_color);
    }

    // Footer styling
    if (footer_bg_) {
        Color footer_color = style->get_variable_color("--footer-bg", color_from_u8(249, 250, 251, 255));
        footer_bg_->set_fill(footer_color);
    }

    // Text styling
    if (title_text_) {
        title_text_->set_color(style->text_color);
    }

    if (subtitle_text_) {
        Color subtitle_color = style->get_variable_color("--subtitle-color", color_from_u8(107, 114, 128, 255));
        subtitle_text_->set_color(subtitle_color);
    }
}

void CardWidget::render(const Element& elem, Renderer& renderer) {
    auto* style = elem.computed_style;
    if (!style) return;

    rebuild_shapes(elem);
    update_shapes(elem);

    Transform world_transform = flex::make_translation(elem.absolute_x(), elem.absolute_y());
    float opacity = style->opacity;

    root_.draw(renderer.flex(), world_transform, opacity);

    dirty_ = false;
}

bool CardWidget::handle_event(const Event& event, Element& elem) {
    (void)event;
    (void)elem;
    return false;
}

void CardWidget::update(float delta_ms, Element& elem) {
    (void)delta_ms;
    (void)elem;
}

} // namespace flexUI

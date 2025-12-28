/*
 * flexUI - AvatarWidget Implementation
 */

#include <flexUI/widgets/avatar_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/renderer.h>
#include <cctype>

namespace flexUI {

AvatarWidget::AvatarWidget(const std::string& name, const std::string& image_url)
    : name_(name), image_url_(image_url) {}

void AvatarWidget::set_name(const std::string& name) {
    name_ = name;
    dirty_ = true;
}

void AvatarWidget::set_image_url(const std::string& url) {
    image_url_ = url;
    dirty_ = true;
}

std::string AvatarWidget::get_initials() const {
    std::string initials;
    bool next_is_initial = true;

    for (char c : name_) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            next_is_initial = true;
        } else if (next_is_initial && std::isalpha(static_cast<unsigned char>(c))) {
            initials += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            next_is_initial = false;
            if (initials.size() >= 2) break;
        }
    }

    if (initials.empty() && !name_.empty()) {
        initials = name_.substr(0, 1);
        initials[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(initials[0])));
    }

    return initials;
}

void AvatarWidget::rebuild_shapes(const Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    float size = style->get_variable_float("--avatar-size", std::min(elem.width(), elem.height()));
    if (size == cached_size_ && background_ != nullptr) {
        return;
    }

    root_.clear();
    cached_size_ = size;

    float radius;
    if (avatar_shape_ == AvatarShape::Circle) {
        radius = size / 2;
    } else if (avatar_shape_ == AvatarShape::Square) {
        radius = 0;
    } else {
        radius = size * 0.15f;
    }

    // Background
    float x = (elem.width() - size) / 2;
    float y = (elem.height() - size) / 2;
    background_ = root_.add<RectShape>(x, y, size, size, radius);

    // Initials text
    std::string initials = get_initials();
    if (!initials.empty()) {
        float font_size = size * 0.4f;
        float char_width = font_size * 0.6f;
        float text_width = initials.size() * char_width;
        float text_x = x + (size - text_width) / 2;
        float text_y = y + (size - font_size) / 2;

        initials_text_ = root_.add<TextShape>(text_x, text_y, initials);
        initials_text_->set_font_size(font_size);
        initials_text_->set_font_family(style->font_family);
    }

    // Status indicator
    if (status_ != Status::None) {
        float status_size = style->get_variable_float("--status-size", 12.0f);
        float status_x = x + size - status_size / 2;
        float status_y = y + size - status_size / 2;
        status_indicator_ = root_.add<CircleShape>(status_x, status_y, status_size / 2);
    }
}

void AvatarWidget::update_shapes(const Element& elem) {
    auto* style = elem.computed_style;
    if (!style || !background_) return;

    Color bg_color = style->get_variable_color("--avatar-bg", color_from_u8(229, 231, 235, 255));
    Color text_color = style->get_variable_color("--avatar-text", color_from_u8(107, 114, 128, 255));
    Color border_color = style->get_variable_color("--avatar-border", color_from_u8(255, 255, 255, 255));
    float border_width = style->get_variable_float("--avatar-border-width", 0.0f);

    background_->set_fill(bg_color);
    if (border_width > 0) {
        background_->set_stroke(border_color, border_width);
    }

    if (initials_text_) {
        initials_text_->set_color(text_color);
    }

    if (status_indicator_) {
        Color status_color;
        switch (status_) {
            case Status::Online:
                status_color = style->get_variable_color("--status-online", color_from_u8(34, 197, 94, 255));
                break;
            case Status::Offline:
                status_color = style->get_variable_color("--status-offline", color_from_u8(156, 163, 175, 255));
                break;
            case Status::Away:
                status_color = style->get_variable_color("--status-away", color_from_u8(234, 179, 8, 255));
                break;
            case Status::Busy:
                status_color = style->get_variable_color("--status-busy", color_from_u8(239, 68, 68, 255));
                break;
            default:
                status_color = color_from_u8(0, 0, 0, 0);
                break;
        }
        status_indicator_->set_fill(status_color);
        status_indicator_->set_stroke(color_from_u8(255, 255, 255, 255), 2.0f);
    }
}

void AvatarWidget::render(const Element& elem, Renderer& renderer) {
    auto* style = elem.computed_style;
    if (!style) return;

    rebuild_shapes(elem);
    update_shapes(elem);

    Transform world_transform = flex::make_translation(elem.absolute_x(), elem.absolute_y());
    float opacity = style->opacity;

    root_.draw(renderer.flex(), world_transform, opacity);

    dirty_ = false;
}

bool AvatarWidget::handle_event(const Event& event, Element& elem) {
    (void)event;
    (void)elem;
    return false;
}

void AvatarWidget::update(float delta_ms, Element& elem) {
    (void)delta_ms;
    (void)elem;
}

} // namespace flexUI

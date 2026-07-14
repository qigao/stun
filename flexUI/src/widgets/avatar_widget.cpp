/*
 * flexUI - AvatarWidget Implementation
 */

#include <flexUI/widgets/avatar_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/render_command.h>
#include <flexUI/text_layout.h>
#include <cctype>

namespace flexUI {

namespace {

bool is_svg_source(const std::string& src) {
    if (src.size() < 4) return false;
    const size_t suffix_pos = src.size() - 4;
    const char c0 = static_cast<char>(std::tolower(static_cast<unsigned char>(src[suffix_pos + 0])));
    const char c1 = static_cast<char>(std::tolower(static_cast<unsigned char>(src[suffix_pos + 1])));
    const char c2 = static_cast<char>(std::tolower(static_cast<unsigned char>(src[suffix_pos + 2])));
    const char c3 = static_cast<char>(std::tolower(static_cast<unsigned char>(src[suffix_pos + 3])));
    return c0 == '.' && c1 == 's' && c2 == 'v' && c3 == 'g';
}

bool can_render_avatar_image(const flex::RendererCapabilities& caps,
                             const std::string& image_url) {
    if (image_url.empty()) return false;
    if (is_svg_source(image_url)) {
        return caps.svg_images;
    }
    return caps.raster_images;
}

} // namespace

AvatarWidget::AvatarWidget(const std::string& name, const std::string& image_url)
    : name_(name), image_url_(image_url) {}

void AvatarWidget::set_name(const std::string& name) {
    name_ = name;
    sync_host_semantics();
    invalidate_render_cache();
}

void AvatarWidget::set_image_url(const std::string& url) {
    image_url_ = url;
    sync_host_semantics();
    invalidate_render_cache();
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

void AvatarWidget::sync_host_semantics() {
    set_host_attribute("role", "img");

    const std::string initials = get_initials();
    if (!name_.empty()) {
        set_host_attribute("aria-label", name_);
    } else if (!initials.empty()) {
        set_host_attribute("aria-label", initials);
    } else {
        set_host_attribute("aria-label", "Avatar");
    }

    if (!image_url_.empty()) {
        set_host_attribute("data-state", "image");
    } else if (!initials.empty()) {
        set_host_attribute("data-state", "fallback");
    } else {
        set_host_attribute("data-state", "empty");
    }

    switch (status_) {
        case Status::Online:
            set_host_attribute("data-status", "online");
            break;
        case Status::Offline:
            set_host_attribute("data-status", "offline");
            break;
        case Status::Away:
            set_host_attribute("data-status", "away");
            break;
        case Status::Busy:
            set_host_attribute("data-status", "busy");
            break;
        case Status::None:
        default:
            clear_host_attribute("data-status");
            break;
    }

    switch (avatar_shape_) {
        case AvatarShape::Square:
            set_host_attribute("data-shape", "square");
            break;
        case AvatarShape::Rounded:
            set_host_attribute("data-shape", "rounded");
            break;
        case AvatarShape::Circle:
        default:
            set_host_attribute("data-shape", "circle");
            break;
    }
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
        ComputedStyle initials_style;
        initials_style.font_size = font_size;
        initials_style.font_family = style->font_family;
        initials_style.font_weight = style->font_weight;
        const float text_width =
            approximate_segmented_text_width(&initials_style, initials);
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

bool AvatarWidget::render_cache_matches(const Element& elem,
                                        const flex::RendererCapabilities& caps) const {
    const auto* style = elem.computed_style;
    if (!render_cache_valid_ || !style) {
        return false;
    }

    return cached_name_ == name_ &&
           cached_image_url_ == image_url_ &&
           cached_raster_images_ == caps.raster_images &&
           cached_svg_images_ == caps.svg_images &&
           cached_style_signature_ == style->variables_signature() &&
           cached_status_ == static_cast<int>(status_) &&
           cached_avatar_shape_ == static_cast<int>(avatar_shape_) &&
           cached_width_ == elem.width() &&
           cached_height_ == elem.height() &&
           cached_opacity_ == style->opacity &&
           cached_font_weight_ == static_cast<int>(style->font_weight) &&
           cached_font_family_ == style->font_family;
}

void AvatarWidget::update_render_cache_key(const Element& elem,
                                           const flex::RendererCapabilities& caps) {
    const auto* style = elem.computed_style;
    if (!style) {
        render_cache_valid_ = false;
        return;
    }

    cached_name_ = name_;
    cached_image_url_ = image_url_;
    cached_raster_images_ = caps.raster_images;
    cached_svg_images_ = caps.svg_images;
    cached_style_signature_ = style->variables_signature();
    cached_status_ = static_cast<int>(status_);
    cached_avatar_shape_ = static_cast<int>(avatar_shape_);
    cached_width_ = elem.width();
    cached_height_ = elem.height();
    cached_opacity_ = style->opacity;
    cached_font_weight_ = static_cast<int>(style->font_weight);
    cached_font_family_ = style->font_family;
    render_cache_valid_ = true;
}

void AvatarWidget::invalidate_render_cache() {
    render_cache_valid_ = false;
    dirty_ = true;
}

void AvatarWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
    auto* style = elem.computed_style;
    if (!style) return;
    sync_host_semantics();

    const auto& caps = commands.capabilities();
    if (render_cache_matches(elem, caps)) {
        commands.append_with_transform_prefix(render_cache_);
        dirty_ = false;
        return;
    }

    rebuild_shapes(elem);
    update_shapes(elem);

    const bool render_image = can_render_avatar_image(caps, image_url_);
    if (initials_text_) {
        initials_text_->set_visible(!render_image);
    }

    Transform local_transform = Transform{};
    float opacity = style->opacity;

    RenderCommandList rebuilt(caps);
    root_.draw(rebuilt, local_transform, opacity);

    if (render_image && background_) {
        rebuilt.save();
        rebuilt.set_transform(local_transform);
        if (is_svg_source(image_url_)) {
            rebuilt.draw_svg(image_url_, background_->x(), background_->y(),
                             background_->width(), background_->height());
        } else {
            rebuilt.draw_image(image_url_, background_->x(), background_->y(),
                               background_->width(), background_->height());
        }
        rebuilt.restore();
    }

    render_cache_ = rebuilt.commands();
    update_render_cache_key(elem, caps);
    commands.append_with_transform_prefix(render_cache_);
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

/*
 * Flex Engine - Text Node Implementation
 */
#include "flex/runtime/text.h"
#include "flex/runtime/renderer.h"
#include <algorithm>

namespace flex {

// ============================================================================
// Text Measurement
// ============================================================================

void Text::update_measurement() const {
    if (measurement_valid_) return;

    // Approximate text width based on character count and font size
    // This is a rough estimate; real implementation would use font metrics
    // Average character width is roughly 0.5-0.6 of font size for proportional fonts
    float avg_char_width = font_size_ * 0.55f;

    // Count characters (simple approximation)
    size_t char_count = content_.length();

    // Add letter spacing
    float total_spacing = letter_spacing_ * (std::max)(0, static_cast<int>(char_count) - 1);

    measured_width_ = char_count * avg_char_width + total_spacing;

    // Apply max width constraint
    if (max_width_ > 0 && measured_width_ > max_width_) {
        // Estimate number of lines needed
        int lines = static_cast<int>(std::ceil(measured_width_ / max_width_));
        measured_width_ = max_width_;
        measured_height_ = font_size_ * line_height_ * lines;
    } else {
        // Single line
        measured_height_ = font_size_ * line_height_;
    }

    measurement_valid_ = true;
}

Bounds Text::compute_bounds() const {
    update_measurement();
    return Bounds{0, 0, measured_width_, measured_height_};
}

float Text::measured_width() const {
    update_measurement();
    return measured_width_;
}

float Text::measured_height() const {
    update_measurement();
    return measured_height_;
}

// ============================================================================
// Rendering
// ============================================================================

void Text::render(Renderer& renderer) {
    if (!visible() || content_.empty()) return;

    renderer.save();
    
    // Apply local transform (relative to parent, accumulated by renderer stack)
    renderer.translate(x(), y());
    if (rotation() != 0.0f) {
        renderer.rotate(rotation());
    }
    if (scale_x() != 1.0f || scale_y() != 1.0f) {
        renderer.scale(scale_x(), scale_y());
    }
    
    renderer.set_global_alpha(opacity());

    // Apply shadow if set
    if (has_shadow()) {
        renderer.set_shadow(shadow());
    }

    // Apply blur if set
    if (has_blur()) {
        renderer.set_blur(blur());
    }

    // Calculate alignment offset
    float ox = 0;
    if (text_align_ == TextAlign::Center) {
        ox = -measured_width() / 2.0f;
    } else if (text_align_ == TextAlign::Right) {
        ox = -measured_width();
    }

    renderer.draw_text(content_, ox, 0, font_family_, font_size_,
                       font_weight_ == FontWeight::Bold, color_);

    // Clear effects
    if (has_shadow()) renderer.clear_shadow();
    if (has_blur()) renderer.clear_blur();

    renderer.restore();
}

// ============================================================================
// Animation Property Dispatch
// ============================================================================

static Color get_color_value(const AnimValue& value) {
    if (auto* c = std::get_if<Color>(&value)) {
        return *c;
    } else if (auto* s = std::get_if<std::string>(&value)) {
        return Color::from_hex(s->c_str());
    }
    return Color::Black;
}

bool Text::set_animated_property(PropertyID pid, const AnimValue& value) {
    // Try base class first
    if (Node::set_animated_property(pid, value)) return true;

    switch (pid) {
    case PropertyID::Text:
    case PropertyID::Content:
        if (auto* s = std::get_if<std::string>(&value)) {
            if (content_ != *s) set_content(*s);
            return true;
        }
        break;

    case PropertyID::FontSize:
        if (auto* f = std::get_if<float>(&value)) {
            if (font_size_ != *f) set_font_size(*f);
            return true;
        }
        break;

    case PropertyID::TextColor:
    case PropertyID::Color: {
        Color new_color = get_color_value(value);
        if (color_ != new_color) set_color(new_color);
        return true;
    }

    default:
        break;
    }

    return false;
}

} // namespace flex

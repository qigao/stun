/*
 * flexUI - LabelWidget Implementation
 */

#include <flexUI/widgets/label_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/renderer.h>
#include <algorithm>
#include <sstream>

namespace flexUI {

LabelWidget::LabelWidget(const std::string& text)
    : text_(text) {}

void LabelWidget::set_text(const std::string& text) {
    text_ = text;
    dirty_ = true;
}

void LabelWidget::rebuild_shapes(const Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    // Get display text
    std::string display_text = !elem.text().empty() ? elem.text() : text_;
    if (display_text.empty()) {
        root_.clear();
        text_shape_ = nullptr;
        return;
    }

    // Process text (ellipsis, line clamp, etc.)
    std::string processed_text = process_text(display_text, elem);

    // Check if rebuild needed
    if (processed_text == cached_display_text_ &&
        elem.width() == cached_width_ &&
        elem.height() == cached_height_ &&
        text_shape_ != nullptr) {
        return;
    }

    root_.clear();
    cached_display_text_ = processed_text;
    cached_width_ = elem.width();
    cached_height_ = elem.height();

    if (processed_text.empty()) {
        text_shape_ = nullptr;
        return;
    }

    // Text alignment
    std::string text_align = style->get_variable("--text-align", "left");
    std::string vertical_align = style->get_variable("--vertical-align", "top");

    // Calculate text width (simplified: monospace assumption)
    float char_width = style->font_size * 0.6f;
    float text_width = processed_text.size() * char_width;

    // Base position with padding
    float base_x = style->padding[3];  // left padding
    float base_y = style->padding[0];  // top padding

    // Horizontal alignment
    if (text_align == "center") {
        base_x = (elem.width() - text_width) / 2;
    } else if (text_align == "right") {
        base_x = elem.width() - text_width - style->padding[1];
    }

    // Vertical alignment
    if (vertical_align == "middle") {
        base_y = (elem.height() - style->font_size) / 2;
    } else if (vertical_align == "bottom") {
        base_y = elem.height() - style->padding[2];
    } else {
        base_y += 0;  // top: already at top with padding
    }

    // Create text shape
    text_shape_ = root_.add<TextShape>(base_x, base_y, processed_text);
    text_shape_->set_font_family(style->font_family);
    text_shape_->set_font_size(style->font_size);
    text_shape_->set_bold(static_cast<int>(style->font_weight) >= 700);

    // Text color
    Color text_color = style->get_variable_color("--text-color", style->text_color);
    text_shape_->set_color(text_color);
}

void LabelWidget::render(const Element& elem, Renderer& renderer) {
    auto* style = elem.computed_style;
    if (!style) return;

    // Rebuild shapes if needed
    rebuild_shapes(elem);

    if (!text_shape_) return;

    // Draw using flex::Renderer
    Transform world_transform = flex::make_translation(elem.absolute_x(), elem.absolute_y());

    float opacity = style->opacity;
    root_.draw(renderer.flex(), world_transform, opacity);

    dirty_ = false;
}

bool LabelWidget::handle_event(const Event& event, Element& elem) {
    // Label has no interaction
    return false;
}

void LabelWidget::update(float delta_ms, Element& elem) {
    // Label has no animation
}

std::string LabelWidget::process_text(const std::string& text, const Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return text;

    std::string result = text;

    // 1. white-space handling
    std::string white_space = style->get_variable("--white-space", "normal");
    if (white_space == "nowrap") {
        result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
    }

    // 2. text-overflow handling
    std::string text_overflow = style->get_variable("--text-overflow", "clip");
    if (text_overflow == "ellipsis") {
        float max_width = elem.width() - style->padding[1] - style->padding[3];
        result = apply_ellipsis(result, max_width, style->font_size);
    }

    // 3. max-lines / line-clamp handling
    std::string max_lines_str = style->get_variable("--max-lines", "");
    if (max_lines_str.empty()) {
        max_lines_str = style->get_variable("--line-clamp", "");
    }

    if (!max_lines_str.empty()) {
        int max_lines = std::stoi(max_lines_str);
        if (max_lines > 0) {
            std::istringstream ss(result);
            std::string line;
            std::string limited_text;
            int line_count = 0;

            while (std::getline(ss, line) && line_count < max_lines) {
                if (line_count > 0) limited_text += "\n";
                limited_text += line;
                line_count++;
            }

            if (std::getline(ss, line)) {
                limited_text += "...";
            }

            result = limited_text;
        }
    }

    return result;
}

std::string LabelWidget::apply_ellipsis(const std::string& text, float max_width, float font_size) {
    float char_width = font_size * 0.6f;
    size_t max_chars = static_cast<size_t>(max_width / char_width);

    if (text.size() <= max_chars) {
        return text;
    }

    if (max_chars < 3) return "...";
    return text.substr(0, max_chars - 3) + "...";
}

} // namespace flexUI

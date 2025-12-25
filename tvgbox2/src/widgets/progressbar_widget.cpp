/*
 * tvgbox2 - ProgressBarWidget Implementation
 */

#include <tvgbox2/widgets/progressbar_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/renderer.h>
#include <algorithm>
#include <cmath>

namespace tvgbox2 {

ProgressBarWidget::ProgressBarWidget(float value, bool indeterminate)
    : value_(std::max(0.0f, std::min(100.0f, value))),
      indeterminate_(indeterminate) {}

void ProgressBarWidget::set_value(float value) {
    value_ = std::max(0.0f, std::min(100.0f, value));
    dirty_ = true;
}

void ProgressBarWidget::set_indeterminate(bool indeterminate) {
    if (indeterminate_ != indeterminate) {
        indeterminate_ = indeterminate;
        animation_time_ = 0.0f;
        animation_position_ = 0.0f;
        dirty_ = true;
    }
}

void ProgressBarWidget::rebuild_shapes(const Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    float bar_height = style->get_variable_float("--progress-height", 8.0f);

    // Check if rebuild needed
    if (elem.width() == cached_width_ &&
        elem.height() == cached_height_ &&
        bar_height == cached_bar_height_ &&
        background_ != nullptr) {
        return;
    }

    root_.clear();
    cached_width_ = elem.width();
    cached_height_ = elem.height();
    cached_bar_height_ = bar_height;

    float radius = style->get_variable_float("--progress-border-radius", 4.0f);
    float y = (elem.height() - bar_height) / 2;

    // Background
    background_ = root_.add<RectShape>(0, y, elem.width(), bar_height, radius);

    // Fill (width will be updated in update_shapes)
    fill_ = root_.add<RectShape>(0, y, 0, bar_height, radius);
}

void ProgressBarWidget::update_shapes(const Element& elem) {
    if (!background_ || !fill_) return;

    auto* style = elem.computed_style;
    if (!style) return;

    float bar_height = style->get_variable_float("--progress-height", 8.0f);

    // Colors
    Color bg_color = style->get_variable_color("--progress-bg", color_from_u8(229, 231, 235, 255));
    Color fill_color = style->get_variable_color("--progress-fill", color_from_u8(59, 130, 246, 255));

    background_->set_fill(bg_color);
    fill_->set_fill(fill_color);

    // Calculate fill dimensions based on mode
    bool is_indet = indeterminate_ || elem.has_state("indeterminate");

    if (is_indet) {
        // Indeterminate: moving segment
        float segment_width = elem.width() * 0.3f;
        float x = (elem.width() - segment_width) * animation_position_;
        float y = (elem.height() - bar_height) / 2;
        fill_->set_position(x, y);
        fill_->set_size(segment_width, bar_height);
    } else {
        // Determinate: progress from left
        float fill_width = elem.width() * (value_ / 100.0f);
        float y = (elem.height() - bar_height) / 2;
        fill_->set_position(0, y);
        fill_->set_size(fill_width, bar_height);
    }

    fill_->set_visible(value_ > 0.0f || is_indet);
}

void ProgressBarWidget::render(const Element& elem, Renderer& renderer) {
    auto* style = elem.computed_style;
    if (!style) return;

    // Rebuild shapes if needed
    rebuild_shapes(elem);

    // Update shape properties
    update_shapes(elem);

    // Draw using flex::Renderer
    Transform world_transform = flex::make_translation(elem.absolute_x(), elem.absolute_y());

    float opacity = style->opacity;
    root_.draw(renderer.flex(), world_transform, opacity);

    dirty_ = false;
}

bool ProgressBarWidget::handle_event(const Event& event, Element& elem) {
    // ProgressBar has no interaction
    return false;
}

void ProgressBarWidget::update(float delta_ms, Element& elem) {
    if (!indeterminate_ && !elem.has_state("indeterminate")) {
        return;
    }

    auto* style = elem.computed_style;
    if (!style) return;

    float duration = style->get_variable_float("--indeterminate-animation-duration", 1500.0f);

    animation_time_ += delta_ms;

    // Loop animation
    if (animation_time_ >= duration) {
        animation_time_ -= duration;
    }

    // Calculate position (smooth back-and-forth motion using sine wave)
    float progress = animation_time_ / duration;
    animation_position_ = (std::sin(progress * 2.0f * 3.14159f) + 1.0f) / 2.0f;

    elem.mark_paint_dirty();
}

} // namespace tvgbox2

/*
 * flexUI - ProgressBarWidget Implementation
 */

#include <flexUI/widgets/progressbar_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <algorithm>
#include <cmath>

namespace flexUI {

ProgressBarWidget::ProgressBarWidget(float value, bool indeterminate)
    : value_(std::max(0.0f, std::min(100.0f, value))),
      indeterminate_(indeterminate) {}

void ProgressBarWidget::build_semantic_tree() {
    track_ = create_part("track", "track");
    fill_ = create_part("fill", "fill");
}

void ProgressBarWidget::sync_host_semantics() {
    set_host_attribute("role", "progressbar");
    set_host_attribute("aria-valuemin", "0");
    set_host_attribute("aria-valuemax", "100");
    if (indeterminate_) {
        set_host_attribute("data-state", "indeterminate");
        clear_host_attribute("aria-valuenow");
        set_host_state("indeterminate", true);
        return;
    }

    const int rounded = static_cast<int>(std::round(value_));
    set_host_attribute("aria-valuenow", std::to_string(rounded));
    set_host_attribute("data-state", value_ >= 100.0f ? "complete" : "loading");
    set_host_state("indeterminate", false);
}

void ProgressBarWidget::set_value(float value) {
    const float clamped = std::max(0.0f, std::min(100.0f, value));
    if (value_ == clamped) {
        return;
    }
    value_ = clamped;
    sync_host_semantics();
    invalidate_render_cache();
}

void ProgressBarWidget::set_indeterminate(bool indeterminate) {
    if (indeterminate_ != indeterminate) {
        indeterminate_ = indeterminate;
        animation_time_ = 0.0f;
        animation_position_ = 0.0f;
        sync_host_semantics();
        invalidate_render_cache();
    }
}

void ProgressBarWidget::update_part_geometry(const Element& elem) {
    if (!track_ || !fill_ || !track_->computed_style || !fill_->computed_style) {
        return;
    }
    const auto height = [](const Element& part, float fallback) {
        return part.computed_style->height_size.kind == CssSizeKind::Auto
                   ? fallback
                   : std::max(part.computed_style->height, 0.0f);
    };
    const float legacy_height = elem.computed_style->get_variable_float(
        "--progress-height", 8.0f);
    const float track_height = height(*track_, legacy_height);
    const float fill_height = height(*fill_, track_height);
    const float track_y = (elem.height() - track_height) * 0.5f;
    const float fill_y = (elem.height() - fill_height) * 0.5f;
    track_->set_layout_bounds(0.0f, track_y, elem.width(), track_height);

    const bool is_indeterminate =
        indeterminate_ || elem.has_state("indeterminate");
    if (is_indeterminate) {
        const float segment_width = elem.width() * 0.3f;
        const float x = (elem.width() - segment_width) * animation_position_;
        fill_->set_layout_bounds(x, fill_y, segment_width, fill_height);
    } else {
        fill_->set_layout_bounds(0.0f, fill_y,
                                 elem.width() * (value_ / 100.0f), fill_height);
    }
    fill_->set_visible(value_ > 0.0f || is_indeterminate);
}

void ProgressBarWidget::sync_host_semantics_for_layout(Element& elem) {
    sync_host_semantics();
    update_part_geometry(elem);
}

void ProgressBarWidget::invalidate_render_cache() {
    dirty_ = true;
    if (auto* host = host_element()) {
        host->mark_paint_dirty();
    }
}

void ProgressBarWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
    (void)elem;
    (void)commands;
    dirty_ = false;
}

bool ProgressBarWidget::handle_event(const Event& event, Element& elem) {
    // ProgressBar has no interaction
    return false;
}

bool ProgressBarWidget::needs_frame_update(const Element& elem) const {
    return indeterminate_ || elem.has_state("indeterminate");
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

} // namespace flexUI

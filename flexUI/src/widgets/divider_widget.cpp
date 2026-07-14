/*
 * flexUI - DividerWidget Implementation
 */

#include <flexUI/widgets/divider_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/render_command.h>
#include <flexUI/text_layout.h>
#include <stb_sprintf.h>

namespace flexUI {

DividerWidget::DividerWidget(Orientation orientation) : orientation_(orientation) {}

void DividerWidget::sync_host_semantics() {
  set_host_attribute("role", "separator");
  set_host_attribute("aria-orientation",
                     orientation_ == Orientation::Vertical ? "vertical"
                                                           : "horizontal");
}

void DividerWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
  auto* style = elem.computed_style;

  Color color = {0.78f, 0.78f, 0.78f, 1.0f};  // 200/255
  float thickness = 1.0f;

  if (style) {
    color = style->get_variable_color("--divider-color", color);
    thickness = style->get_variable_float("--divider-thickness", thickness);

    std::string orient_str = style->get_variable("--divider-orientation", "");
    if (orient_str == "vertical") orientation_ = Orientation::Vertical;
    else if (orient_str == "horizontal") orientation_ = Orientation::Horizontal;

    std::string style_str = style->get_variable("--divider-style", "solid");
    if (style_str == "dashed") style_ = Style::Dashed;
    else if (style_str == "dotted") style_ = Style::Dotted;
    else style_ = Style::Solid;
  }

  sync_host_semantics();

  Paint stroke = Paint::solid(color);

  if (orientation_ == Orientation::Horizontal) {
    float y = elem.height() / 2;

    if (label_.empty()) {
      // Simple horizontal line
      char path[128];
      stbsp_snprintf(path, sizeof(path), "M 0 %.4g L %.4g %.4g", y, elem.width(), y);
      commands.stroke_path(path, stroke, thickness);
    } else {
      // Line with gap for label
      float gap = 8.0f;
      float label_width = style ? approximate_segmented_text_width(style, label_)
                                : static_cast<float>(label_.size()) * 8.0f;
      const float available_label_width =
          std::max(0.0f, elem.width() - gap * 2.0f - 24.0f);
      label_width = std::min(label_width, available_label_width);
      float center = elem.width() / 2;

      // Left segment
      char path1[128];
      stbsp_snprintf(path1, sizeof(path1), "M 0 %.4g L %.4g %.4g",
               y, center - label_width / 2 - gap, y);
      commands.stroke_path(path1, stroke, thickness);

      // Right segment
      char path2[128];
      stbsp_snprintf(path2, sizeof(path2), "M %.4g %.4g L %.4g %.4g",
               center + label_width / 2 + gap, y, elem.width(), y);
      commands.stroke_path(path2, stroke, thickness);
      // Render label
      if (style) {
        const auto text_block = layout_text_block(
            style, label_, center - label_width / 2.0f, y - style->font_size,
            label_width, resolve_line_height(style), color, TextVerticalAlign::Top);
        emit_text_block(commands, text_block);
      }
    }
  } else {
    // Vertical line
    float x = elem.width() / 2;
    char path[128];
    stbsp_snprintf(path, sizeof(path), "M %.4g 0 L %.4g %.4g", x, x, elem.height());
    commands.stroke_path(path, stroke, thickness);
  }

  // Dash pattern support belongs in the backend-neutral stroke command.
}

bool DividerWidget::handle_event(const Event& event, Element& elem) {
  return false;
}

void DividerWidget::update(float delta_ms, Element& elem) {
  // No animation
}

} // namespace flexUI

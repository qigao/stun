#include "whiteboard/panels/properties_controller.h"
#include "whiteboard/model/whiteboard_document.h"
#include "whiteboard/properties_panel_module.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace whiteboard {

PropertiesController::PropertiesController(WhiteboardDocument *document,
                                           ::PropertiesPanelModule *view)
    : m_document(document), m_view(view) {}

PropertiesController::~PropertiesController() {}

// === Helper Method ===

void PropertiesController::apply_to_selected(std::function<void(Stroke &)> modifier) {
  if (!m_document)
    return;

  const auto &selected = m_document->get_selected_indices();
  if (selected.empty()) {
    return; // No selection - caller should handle default properties
  }

  const auto &strokes = m_document->get_strokes();

  for (int index : selected) {
    if (index < 0 || index >= static_cast<int>(strokes.size()))
      continue;

    Stroke stroke = strokes[index];

    // Skip locked shapes
    if (stroke.locked)
      continue;

    // Apply modification
    modifier(stroke);

    // Update in document (handles undo and notifications)
    m_document->update_stroke(index, stroke);
  }
}

// === Stroke Properties ===

void PropertiesController::set_stroke_color(const nanogui::Color &color) {
  const auto &selected = m_document->get_selected_indices();

  if (selected.empty()) {
    // No selection - update default tool property
    m_document->set_stroke_color(color);
    return;
  }

  // Apply to selected shapes
  apply_to_selected([&color](Stroke &stroke) { stroke.color = color; });
}

void PropertiesController::set_fill_color(const nanogui::Color &color) {
  const auto &selected = m_document->get_selected_indices();

  if (selected.empty()) {
    // No selection - update default tool property
    m_document->set_fill_color(color);
    return;
  }

  // Apply to selected shapes (skip shapes that don't support fill)
  apply_to_selected([&color](Stroke &stroke) {
    // Lines and arrows don't have fill
    if (stroke.tool == Tool::Line || stroke.tool == Tool::Arrow) {
      return;
    }
    stroke.fill_color = color;
  });
}

void PropertiesController::set_stroke_width(float width) {
  // Clamp to minimum value
  width = std::max(0.5f, width);

  const auto &selected = m_document->get_selected_indices();

  if (selected.empty()) {
    // No selection - update default tool property
    m_document->set_stroke_width(width);
    return;
  }

  // Apply to selected shapes
  apply_to_selected([width](Stroke &stroke) { stroke.width = width; });
}

void PropertiesController::set_stroke_style(StrokeStyle style) {
  const auto &selected = m_document->get_selected_indices();

  // Note: No default stroke style in document yet, so only apply to selection
  if (selected.empty()) {
    return;
  }

  // Apply to selected shapes
  apply_to_selected([style](Stroke &stroke) { stroke.stroke_style = style; });
}

void PropertiesController::set_opacity(float opacity) {
  // Clamp to valid range
  opacity = std::clamp(opacity, 0.0f, 1.0f);

  // Apply to selected shapes only (no default opacity)
  apply_to_selected([opacity](Stroke &stroke) { stroke.opacity = opacity; });
}

void PropertiesController::set_rotation(float rotation) {
  // Normalize to 0-2π range
  const float TWO_PI = 2.0f * M_PI;
  while (rotation < 0.0f)
    rotation += TWO_PI;
  while (rotation >= TWO_PI)
    rotation -= TWO_PI;

  // Apply to selected shapes only (no default rotation)
  apply_to_selected([rotation](Stroke &stroke) { stroke.rotation = rotation; });
}

void PropertiesController::set_corner_radius(float radius) {
  // Clamp to non-negative
  radius = std::max(0.0f, radius);

  // Apply only to rectangles and diamonds
  apply_to_selected([radius](Stroke &stroke) {
    if (stroke.tool == Tool::Rectangle || stroke.tool == Tool::Diamond) {
      stroke.corner_radius = radius;
    }
  });
}

void PropertiesController::set_svg_scale_x(float scale_x) {
  // Clamp to reasonable range
  scale_x = std::clamp(scale_x, 0.1f, 3.0f);

  // Apply only to SVG shapes
  apply_to_selected([scale_x](Stroke &stroke) {
    if (stroke.tool == Tool::SVGShape) {
      stroke.svg_scale_x = scale_x;
    }
  });
}

void PropertiesController::set_svg_scale_y(float scale_y) {
  // Clamp to reasonable range
  scale_y = std::clamp(scale_y, 0.1f, 3.0f);

  // Apply only to SVG shapes
  apply_to_selected([scale_y](Stroke &stroke) {
    if (stroke.tool == Tool::SVGShape) {
      stroke.svg_scale_y = scale_y;
    }
  });
}

// === Text Properties ===

void PropertiesController::set_font_face(const std::string &face) {
  const auto &selected = m_document->get_selected_indices();

  if (selected.empty()) {
    // No selection - update default tool property
    m_document->set_font_face(face);
    return;
  }

  // Apply only to text shapes
  apply_to_selected([&face](Stroke &stroke) {
    if (stroke.tool == Tool::Text) {
      stroke.font_face = face;
    }
  });
}

void PropertiesController::set_font_size(float size) {
  // Clamp to reasonable range
  size = std::clamp(size, 8.0f, 72.0f);

  const auto &selected = m_document->get_selected_indices();

  if (selected.empty()) {
    // No selection - update default tool property
    m_document->set_font_size(size);
    return;
  }

  // Apply only to text shapes
  apply_to_selected([size](Stroke &stroke) {
    if (stroke.tool == Tool::Text) {
      stroke.font_size = size;
    }
  });
}

void PropertiesController::set_text_align(int align) {
  const auto &selected = m_document->get_selected_indices();

  if (selected.empty()) {
    // No selection - update default tool property
    m_document->set_text_align(align);
    return;
  }

  // Apply only to text shapes
  apply_to_selected([align](Stroke &stroke) {
    if (stroke.tool == Tool::Text) {
      stroke.text_align = align;
    }
  });
}

// === Layer Operations ===

void PropertiesController::bring_forward() {
  if (!m_document)
    return;

  auto selected = m_document->get_selected_indices();
  if (selected.empty())
    return;

  // Sort in descending order to avoid index conflicts
  std::sort(selected.begin(), selected.end(), std::greater<int>());

  for (int index : selected) {
    int target = std::min(index + 1, static_cast<int>(m_document->get_strokes().size()) - 1);
    if (index != target) {
      m_document->reorder_stroke(index, target);
    }
  }
}

void PropertiesController::send_backward() {
  if (!m_document)
    return;

  auto selected = m_document->get_selected_indices();
  if (selected.empty())
    return;

  // Sort in ascending order to avoid index conflicts
  std::sort(selected.begin(), selected.end());

  for (int index : selected) {
    int target = std::max(index - 1, 0);
    if (index != target) {
      m_document->reorder_stroke(index, target);
    }
  }
}

void PropertiesController::bring_to_front() {
  if (!m_document)
    return;

  auto selected = m_document->get_selected_indices();
  if (selected.empty())
    return;

  // Sort in descending order to avoid index conflicts
  std::sort(selected.begin(), selected.end(), std::greater<int>());

  for (int index : selected) {
    int target = static_cast<int>(m_document->get_strokes().size()) - 1;
    if (index != target) {
      m_document->reorder_stroke(index, target);
    }
  }
}

void PropertiesController::send_to_back() {
  if (!m_document)
    return;

  auto selected = m_document->get_selected_indices();
  if (selected.empty())
    return;

  // Sort in ascending order to avoid index conflicts
  std::sort(selected.begin(), selected.end());

  for (int index : selected) {
    if (index != 0) {
      m_document->reorder_stroke(index, 0);
    }
  }
}

} // namespace whiteboard

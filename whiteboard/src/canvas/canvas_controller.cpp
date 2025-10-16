/**
 * \file canvas_controller.cpp
 * \brief Implementation of CanvasController class.
 */

#include "whiteboard/canvas/canvas_controller.h"
#include "whiteboard/canvas/canvas_view.h"
#include "whiteboard/svg/svg_parameter_editor.h"
#include "whiteboard/svg/svg_shape_library.h"
#include <cmath>
#include <chrono>
#include <iostream>

namespace whiteboard {

CanvasController::CanvasController(WhiteboardDocument *document, CanvasView *view)
    : m_document(document), m_view(view), m_shape_library(nullptr), 
      m_mode(Mode::None), m_last_clicked_stroke(-1) {
  m_last_click_time = std::chrono::steady_clock::now();
}

// === Input Handling ===

bool CanvasController::handle_mouse_down(const nanogui::Vector2f &canvas_pos, int modifiers) {
  Tool current_tool = m_document->get_current_tool();

  // Route to appropriate tool handler
  switch (current_tool) {
  case Tool::Pen:
    handle_pen_tool_down(canvas_pos);
    return true;

  case Tool::Rectangle:
  case Tool::Circle:
  case Tool::Line:
  case Tool::Arrow:
  case Tool::Diamond:
    handle_shape_tool_down(canvas_pos);
    return true;

  case Tool::Select:
    handle_select_tool_down(canvas_pos, modifiers);
    return true;

  case Tool::Pan:
    handle_pan_tool_down(canvas_pos);
    return true;

  case Tool::SVGShape:
    handle_svg_shape_tool_down(canvas_pos);
    return true;

  case Tool::Text:
  case Tool::Sticky:
  case Tool::Image:
    // TODO: Implement these tools
    return false;

  default:
    return false;
  }
}

bool CanvasController::handle_mouse_drag(const nanogui::Vector2f &canvas_pos, int modifiers) {
  // Handle based on current mode
  switch (m_mode) {
  case Mode::Drawing:
    if (m_document->get_current_tool() == Tool::Pen) {
      handle_pen_tool_drag(canvas_pos);
    } else if (m_document->get_current_tool() == Tool::SVGShape) {
      handle_svg_shape_tool_drag(canvas_pos);
    } else {
      handle_shape_tool_drag(canvas_pos);
    }
    return true;

  case Mode::AreaSelecting:
  case Mode::Moving:
  case Mode::Resizing:
  case Mode::Rotating:
    handle_select_tool_drag(canvas_pos);
    return true;

  case Mode::Panning:
    handle_pan_tool_drag(canvas_pos);
    return true;

  default:
    return false;
  }
}

bool CanvasController::handle_mouse_up(const nanogui::Vector2f &canvas_pos, int modifiers) {
  // Handle based on current mode
  switch (m_mode) {
  case Mode::Drawing:
    if (m_document->get_current_tool() == Tool::Pen) {
      handle_pen_tool_up(canvas_pos);
    } else if (m_document->get_current_tool() == Tool::SVGShape) {
      handle_svg_shape_tool_up(canvas_pos);
    } else {
      handle_shape_tool_up(canvas_pos);
    }
    m_mode = Mode::None;
    return true;

  case Mode::AreaSelecting:
  case Mode::Moving:
  case Mode::Resizing:
  case Mode::Rotating:
    handle_select_tool_up(canvas_pos);
    m_mode = Mode::None;
    return true;

  case Mode::Panning:
    handle_pan_tool_up(canvas_pos);
    m_mode = Mode::None;
    return true;

  default:
    m_mode = Mode::None;
    return false;
  }
}

bool CanvasController::handle_scroll(const nanogui::Vector2f &canvas_pos, float delta) {
  // Zoom in/out
  float zoom_factor = delta > 0 ? 1.1f : 0.9f;
  float new_zoom = m_document->get_zoom() * zoom_factor;

  // Clamp zoom
  new_zoom = std::max(0.25f, std::min(6.0f, new_zoom));

  m_document->set_zoom(new_zoom);
  return true;
}

bool CanvasController::handle_key_press(int key, int modifiers) {
  // TODO: Implement keyboard shortcuts (delete, undo, redo, etc.)
  return false;
}

bool CanvasController::handle_key_release(int key, int modifiers) {
  // TODO: Implement key release handling (e.g., space for pan)
  return false;
}

// === Tool Handlers ===

void CanvasController::handle_pen_tool_down(const nanogui::Vector2f &pos) {
  // Start drawing a new pen stroke
  m_mode = Mode::Drawing;

  // Create new stroke with current properties
  m_temp_stroke = Stroke();
  m_temp_stroke.tool = Tool::Pen;
  m_temp_stroke.color = m_document->get_stroke_color();
  m_temp_stroke.width = m_document->get_stroke_width();
  m_temp_stroke.fill_style = m_document->get_fill_style();
  m_temp_stroke.fill_color = m_document->get_fill_color();

  // Add first point (with snapping)
  Point snapped = snap_point(Point(pos.x(), pos.y()));
  m_temp_stroke.points.push_back(snapped);

  // Update view to show temporary stroke
  m_view->set_current_stroke(m_temp_stroke);
}

void CanvasController::handle_pen_tool_drag(const nanogui::Vector2f &pos) {
  // Add point to current stroke (with snapping)
  Point snapped = snap_point(Point(pos.x(), pos.y()));
  m_temp_stroke.points.push_back(snapped);

  // Update view
  m_view->set_current_stroke(m_temp_stroke);
}

void CanvasController::handle_pen_tool_up(const nanogui::Vector2f &pos) {
  // Finalize the stroke
  if (!m_temp_stroke.points.empty()) {
    m_document->add_stroke(m_temp_stroke);
  }

  // Clear temporary stroke from view
  m_view->clear_current_stroke();
}

void CanvasController::handle_shape_tool_down(const nanogui::Vector2f &pos) {
  // Start drawing a shape
  m_mode = Mode::Drawing;
  m_interaction_start = pos;

  // Create new stroke with current properties
  m_temp_stroke = Stroke();
  m_temp_stroke.tool = m_document->get_current_tool();
  m_temp_stroke.color = m_document->get_stroke_color();
  m_temp_stroke.width = m_document->get_stroke_width();
  m_temp_stroke.fill_style = m_document->get_fill_style();
  m_temp_stroke.fill_color = m_document->get_fill_color();

  // Add start point (with snapping)
  Point snapped = snap_point(Point(pos.x(), pos.y()));
  m_temp_stroke.points.push_back(snapped);
  m_temp_stroke.points.push_back(snapped); // End point (will be updated on drag)

  // Update view
  m_view->set_current_stroke(m_temp_stroke);
}

void CanvasController::handle_shape_tool_drag(const nanogui::Vector2f &pos) {
  // Update end point of shape (with snapping)
  Point snapped = snap_point(Point(pos.x(), pos.y()));

  if (m_temp_stroke.points.size() >= 2) {
    m_temp_stroke.points[1] = snapped;
  }

  // Update view
  m_view->set_current_stroke(m_temp_stroke);
}

void CanvasController::handle_shape_tool_up(const nanogui::Vector2f &pos) {
  // Finalize the shape
  if (m_temp_stroke.points.size() >= 2) {
    m_document->add_stroke(m_temp_stroke);
  }

  // Clear temporary stroke from view
  m_view->clear_current_stroke();
}

void CanvasController::handle_select_tool_down(const nanogui::Vector2f &pos, int modifiers) {
  // Check if clicking on an existing stroke
  int clicked_stroke = m_document->find_stroke_at_point(pos.x(), pos.y());

  bool is_shift = (modifiers & 1) != 0; // SHIFT modifier

  if (clicked_stroke >= 0) {
    // Check for double-click on SVG shape
    if (is_double_click(pos, clicked_stroke)) {
      const auto& strokes = m_document->get_strokes();
      if (clicked_stroke < static_cast<int>(strokes.size()) && 
          strokes[clicked_stroke].tool == Tool::SVGShape) {
        handle_svg_double_click(clicked_stroke, pos);
        return;
      }
    }
    // Clicked on a stroke
    if (is_shift) {
      // Multi-select: toggle selection
      auto selected = m_document->get_selected_indices();
      auto it = std::find(selected.begin(), selected.end(), clicked_stroke);
      if (it != selected.end()) {
        // Already selected, remove it
        selected.erase(it);
      } else {
        // Not selected, add it
        selected.push_back(clicked_stroke);
      }
      m_document->set_selection(selected);
    } else {
      // Single select
      if (!m_document->is_selected(clicked_stroke)) {
        m_document->set_selection({clicked_stroke});
      }
    }

    // Start moving mode
    m_mode = Mode::Moving;
    m_interaction_start = pos;
  } else {
    // Clicked on empty space - start area selection
    if (!is_shift) {
      m_document->clear_selection();
    }

    m_mode = Mode::AreaSelecting;
    m_interaction_start = pos;
    m_view->set_selection_marquee(pos, pos);
  }
}

void CanvasController::handle_select_tool_drag(const nanogui::Vector2f &pos) {
  if (m_mode == Mode::AreaSelecting) {
    // Update marquee
    m_view->set_selection_marquee(m_interaction_start, pos);
  } else if (m_mode == Mode::Moving) {
    // Move selected strokes
    nanogui::Vector2f delta = pos - m_interaction_start;

    const auto &selected = m_document->get_selected_indices();
    const auto &strokes = m_document->get_strokes();

    for (int idx : selected) {
      if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
        Stroke modified = strokes[idx];
        modified.move(delta.x(), delta.y());
        m_document->update_stroke(idx, modified);
      }
    }

    m_interaction_start = pos;
  }
}

void CanvasController::handle_select_tool_up(const nanogui::Vector2f &pos) {
  if (m_mode == Mode::AreaSelecting) {
    // Select all strokes in the marquee rectangle
    float min_x = std::min(m_interaction_start.x(), pos.x());
    float max_x = std::max(m_interaction_start.x(), pos.x());
    float min_y = std::min(m_interaction_start.y(), pos.y());
    float max_y = std::max(m_interaction_start.y(), pos.y());

    auto strokes_in_rect = m_document->find_strokes_in_rect(min_x, min_y, max_x, max_y);
    m_document->set_selection(strokes_in_rect);

    // Clear marquee
    m_view->clear_selection_marquee();
  }
}

void CanvasController::handle_pan_tool_down(const nanogui::Vector2f &pos) {
  m_mode = Mode::Panning;
  m_interaction_start = pos;
  m_rotation_center = m_document->get_pan_offset(); // Store original pan offset
}

void CanvasController::handle_pan_tool_drag(const nanogui::Vector2f &pos) {
  // Calculate delta in canvas coordinates
  nanogui::Vector2f delta = pos - m_interaction_start;

  // Update pan offset (delta is already in canvas space, so we add it directly)
  nanogui::Vector2f new_pan = m_rotation_center + delta;
  m_document->set_pan_offset(new_pan);
}

void CanvasController::handle_pan_tool_up(const nanogui::Vector2f &pos) {
  // Panning complete
}

void CanvasController::handle_svg_shape_tool_down(const nanogui::Vector2f &pos) {
  // Start placing an SVG shape
  m_mode = Mode::Drawing;
  m_interaction_start = pos;

  // Create new stroke for SVG shape
  m_temp_stroke = Stroke();
  m_temp_stroke.tool = Tool::SVGShape;
  m_temp_stroke.color = m_document->get_stroke_color();
  m_temp_stroke.width = m_document->get_stroke_width();
  
  // For now, create a placeholder SVG (will be replaced by shape library)
  m_temp_stroke.svg_data = R"(<svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
    <rect x="10" y="10" width="80" height="80" fill="lightblue" stroke="black" stroke-width="2"/>
  </svg>)";
  m_temp_stroke.svg_shape_id = "basic.rectangle";
  m_temp_stroke.svg_scale_x = 1.0f;
  m_temp_stroke.svg_scale_y = 1.0f;

  // Add position point (with snapping)
  Point snapped = snap_point(Point(pos.x(), pos.y()));
  m_temp_stroke.points.push_back(snapped);
  m_temp_stroke.points.push_back(snapped); // End point for sizing

  // Update view
  m_view->set_current_stroke(m_temp_stroke);
}

void CanvasController::handle_svg_shape_tool_drag(const nanogui::Vector2f &pos) {
  // Update size/scale of SVG shape based on drag
  Point snapped = snap_point(Point(pos.x(), pos.y()));

  if (m_temp_stroke.points.size() >= 2) {
    m_temp_stroke.points[1] = snapped;
    
    // Calculate scale based on drag distance
    float dx = snapped.x - m_temp_stroke.points[0].x;
    float dy = snapped.y - m_temp_stroke.points[0].y;
    
    // Update scale (minimum 0.1 to avoid invisible shapes)
    m_temp_stroke.svg_scale_x = std::max(0.1f, std::abs(dx) / 100.0f);
    m_temp_stroke.svg_scale_y = std::max(0.1f, std::abs(dy) / 100.0f);
  }

  // Update view
  m_view->set_current_stroke(m_temp_stroke);
}

void CanvasController::handle_svg_shape_tool_up(const nanogui::Vector2f &pos) {
  // Finalize the SVG shape
  if (m_temp_stroke.points.size() >= 1) {
    // If barely dragged, use default scale
    if (m_temp_stroke.svg_scale_x < 0.2f) {
      m_temp_stroke.svg_scale_x = 1.0f;
    }
    if (m_temp_stroke.svg_scale_y < 0.2f) {
      m_temp_stroke.svg_scale_y = 1.0f;
    }
    
    m_document->add_stroke(m_temp_stroke);
  }

  // Clear temporary stroke from view
  m_view->clear_current_stroke();
}

// === Helper Methods ===

Point CanvasController::snap_point(const Point &p) {
  if (!m_document->get_snap_enabled()) {
    return p;
  }

  // Snap to grid
  float grid_size = 20.0f; // Should match GRID_SIZE in CanvasView
  float snapped_x = std::round(p.x / grid_size) * grid_size;
  float snapped_y = std::round(p.y / grid_size) * grid_size;

  return Point(snapped_x, snapped_y);
}

bool CanvasController::is_clicking_selection_handle(const nanogui::Vector2f &pos) {
  // TODO: Implement proper handle detection
  // This requires calculating handle positions based on selection bounds
  return false;
}

bool CanvasController::is_clicking_rotation_handle(const nanogui::Vector2f &pos) {
  // TODO: Implement proper rotation handle detection
  // This requires calculating rotation handle position based on selection bounds
  return false;
}

bool CanvasController::is_double_click(const nanogui::Vector2f &pos, int stroke_index) {
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_click_time);
  
  // Double-click threshold: 500ms and within 5 pixels
  bool is_double = (elapsed.count() < 500) && 
                   (m_last_clicked_stroke == stroke_index) &&
                   (std::abs(pos.x() - m_last_click_pos.x()) < 5.0f) &&
                   (std::abs(pos.y() - m_last_click_pos.y()) < 5.0f);
  
  // Update last click info
  m_last_click_time = now;
  m_last_click_pos = pos;
  m_last_clicked_stroke = stroke_index;
  
  return is_double;
}

void CanvasController::handle_svg_double_click(int stroke_index, const nanogui::Vector2f &pos) {
  std::cout << "Double-click detected on SVG shape at index " << stroke_index << std::endl;
  
  // Get the stroke to check if it has a shape library definition
  const auto &strokes = m_document->get_strokes();
  if (stroke_index < 0 || stroke_index >= static_cast<int>(strokes.size())) {
    return;
  }
  
  const Stroke &stroke = strokes[stroke_index];
  if (stroke.svg_shape_id.empty()) {
    std::cout << "SVG shape has no shape_id, cannot edit parameters" << std::endl;
    return;
  }
  
  if (!m_shape_library) {
    std::cout << "No shape library available for editing" << std::endl;
    return;
  }
  
  // Create and show parameter editor
  auto *editor = new SVGParameterEditor(m_view->parent(), m_document, m_shape_library, stroke_index);
  editor->show();
}

} // namespace whiteboard

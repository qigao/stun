/**
 * \file canvas_controller.cpp
 * \brief Implementation of CanvasController class.
 */

#include "whiteboard/canvas/canvas_controller.h"
#include "whiteboard/canvas/canvas_view.h"
#include "whiteboard/canvas/inline_text_editor.h"
#include "whiteboard/common.h"
#include "whiteboard/svg/svg_parameter_editor.h"
#include "whiteboard/svg/svg_shape_library.h"
#include <chrono>
#include <cmath>
#include <fmtlog.h>
#include <iomanip>
#include <iostream>
#include <nanogui/keys.h>
#include <sstream>

namespace whiteboard {

CanvasController::CanvasController(WhiteboardDocument *document, CanvasView *view)
    : m_document(document), m_view(view), m_shape_library(nullptr), m_mode(Mode::None),
      m_last_clicked_stroke(-1), m_temp_guide_position(0.0f), m_dragging_guide_index(-1) {
  m_last_click_time = std::chrono::steady_clock::now();
}

// === Input Handling ===

bool CanvasController::handle_mouse_down(const nanogui::Vector2f &canvas_pos, int modifiers) {
  Tool current_tool = m_document->get_current_tool();
  logi("Mouse DOWN at canvas ({:.1f}, {:.1f}), tool: {}, modifiers: {}", canvas_pos.x(),
       canvas_pos.y(), static_cast<int>(current_tool), modifiers);

  // Ctrl+Click in any mode: select shape under cursor for quick property editing
  bool ctrl_pressed = NANOGUI_HAS_CTRL(modifiers);
  if (ctrl_pressed && current_tool != Tool::Select) {
    logi("Ctrl+Click detected - selecting shape at cursor");
    int clicked_stroke = m_document->find_stroke_at_point(canvas_pos.x(), canvas_pos.y());
    if (clicked_stroke >= 0) {
      logi("   Found stroke {} at cursor", clicked_stroke);

      // Select the stroke
      m_document->set_selection({clicked_stroke});

      logi("   Stroke selected, properties panel should update");
      return true;
    } else {
      logi("   No stroke found at cursor");
    }
  }

  // Check if clicking on a guide (only when guides are visible)
  if (m_document->get_guides_visible()) {
    int guide_index = find_guide_at_point(canvas_pos);
    if (guide_index >= 0) {
      // Start dragging the guide
      logi("📏 Clicked on guide {}, starting drag", guide_index);
      m_mode = Mode::CreatingGuide;
      m_dragging_guide_index = guide_index;
      const auto &guides = m_document->get_guides();
      m_guide_type = guides[guide_index].type;
      m_temp_guide_position = guides[guide_index].position;
      m_interaction_start = canvas_pos;
      m_view->set_temp_guide(m_guide_type, m_temp_guide_position);

      // Remove the guide from document (will be re-added on mouse up if not deleted)
      m_document->remove_guide(guide_index);
      return true;
    }
  }

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
    handle_select_tool_drag(canvas_pos, modifiers);
    return true;

  case Mode::Panning:
    handle_pan_tool_drag(canvas_pos);
    return true;

  case Mode::CreatingGuide:
    // Update temporary guide position
    if (m_guide_type == Guide::Horizontal) {
      m_temp_guide_position = canvas_pos.y();
    } else {
      m_temp_guide_position = canvas_pos.x();
    }
    m_view->set_temp_guide(m_guide_type, m_temp_guide_position);
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

  case Mode::CreatingGuide:
    // Finalize guide creation or deletion
    {
      bool should_delete = false;

      // Check if guide was dragged back to ruler (for deletion)
      if (m_dragging_guide_index >= 0) {
        // This was an existing guide being dragged
        // Delete if dragged back close to ruler edge
        if (m_guide_type == Guide::Horizontal) {
          // Check if close to top edge (within 30 pixels of start)
          should_delete = std::abs(canvas_pos.y() - m_interaction_start.y()) < 30.0f;
        } else {
          // Check if close to left edge (within 30 pixels of start)
          should_delete = std::abs(canvas_pos.x() - m_interaction_start.x()) < 30.0f;
        }
      } else {
        // This was a new guide being created from ruler
        // Calculate distance from ruler to determine if guide should be created
        float distance_from_start;
        if (m_guide_type == Guide::Horizontal) {
          distance_from_start = std::abs(canvas_pos.y() - m_interaction_start.y());
        } else {
          distance_from_start = std::abs(canvas_pos.x() - m_interaction_start.x());
        }

        // Only create guide if dragged far enough from ruler (at least 50 pixels)
        should_delete = distance_from_start < 50.0f;
      }

      // Create guide if not deleted
      if (!should_delete) {
        Guide new_guide(m_guide_type, m_temp_guide_position);
        m_document->add_guide(new_guide);
      }

      // Clear temporary guide
      m_view->clear_temp_guide();
      m_dragging_guide_index = -1;
      m_mode = Mode::None;
    }
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
  // Check modifiers directly (bit 1 = Shift, bit 2 = Ctrl, bit 3 = Alt)
  bool ctrl = (modifiers & 2) != 0;  // Ctrl is bit 2
  bool shift = (modifiers & 1) != 0; // Shift is bit 1

  logi("CanvasController: key={}, modifiers={}, ctrl={}, shift={}", key, modifiers, ctrl, shift);
  if (ctrl && (key == 'C' || key == 'c' || key == 99)) {
    copy_selected_shapes();
    return true;
  }

  // Ctrl+X: Cut selected shapes
  if (ctrl && (key == 'X' || key == 'x' || key == 120)) {
    cut_selected_shapes();
    return true;
  }

  // Ctrl+V: Paste shapes
  if (ctrl && (key == 'V' || key == 'v' || key == 118)) {
    paste_shapes();
    return true;
  }

  // Ctrl+G: Group selected shapes
  if (ctrl && !shift && (key == 'G' || key == 'g' || key == 103)) {
    group_selected_shapes();
    return true;
  }

  // Ctrl+Shift+G: Ungroup selected shapes
  if (ctrl && shift && (key == 'G' || key == 'g' || key == 103)) {
    ungroup_selected_shapes();
    return true;
  }

  // Ctrl+D: Duplicate selected shapes
  if (ctrl && (key == 'D' || key == 'd' || key == 100)) {
    duplicate_selected_shapes();
    return true;
  }

  // Delete: Remove selected shapes
  if (key == NANOGUI_KEY_DELETE || key == NANOGUI_KEY_BACKSPACE) {
    delete_selected_shapes();
    return true;
  }

  // Ctrl+Z: Undo
  if (ctrl && (key == 'Z' || key == 'z' || key == 122)) {
    m_document->undo();
    return true;
  }

  // Ctrl+Y or Ctrl+Shift+Z: Redo
  if (ctrl && ((key == 'Y' || key == 'y' || key == 121) ||
               (shift && (key == 'Z' || key == 'z' || key == 122)))) {
    m_document->redo();
    return true;
  }

  // Alignment shortcuts (Ctrl+Shift+Arrow) - check first before nudge
  if (ctrl && shift) {
    if (key == NANOGUI_KEY_LEFT || key == 263) { // Ctrl+Shift+Left = Align Left
      align_selection_left();
      return true;
    }
    if (key == NANOGUI_KEY_RIGHT || key == 262) { // Ctrl+Shift+Right = Align Right
      align_selection_right();
      return true;
    }
    if (key == NANOGUI_KEY_UP || key == 265) { // Ctrl+Shift+Up = Align Top
      align_selection_top();
      return true;
    }
    if (key == NANOGUI_KEY_DOWN || key == 264) { // Ctrl+Shift+Down = Align Bottom
      align_selection_bottom();
      return true;
    }
  }

  // Arrow keys: Nudge selected shapes (only if not Ctrl)
  // Shift+Arrow = 10px, normal Arrow = 1px
  if (!ctrl) {
    float nudge_amount = shift ? 10.0f : 1.0f;

    if (key == NANOGUI_KEY_LEFT || key == 263) { // Left arrow
      nudge_selection(-nudge_amount, 0.0f);
      return true;
    }

    if (key == NANOGUI_KEY_RIGHT || key == 262) { // Right arrow
      nudge_selection(nudge_amount, 0.0f);
      return true;
    }

    if (key == NANOGUI_KEY_UP || key == 265) { // Up arrow
      nudge_selection(0.0f, -nudge_amount);
      return true;
    }

    if (key == NANOGUI_KEY_DOWN || key == 264) { // Down arrow
      nudge_selection(0.0f, nudge_amount);
      return true;
    }
  }

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
    logi("Pen tool UP - Adding stroke with {} points", m_temp_stroke.points.size());
    m_document->add_stroke(m_temp_stroke);
  } else {
    logd("Pen tool UP - No points to add");
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
    const char *shape_name = "Unknown";
    switch (m_temp_stroke.tool) {
    case Tool::Rectangle:
      shape_name = "Rectangle";
      break;
    case Tool::Circle:
      shape_name = "Circle";
      break;
    case Tool::Diamond:
      shape_name = "Diamond";
      break;
    case Tool::Line:
      shape_name = "Line";
      break;
    case Tool::Arrow:
      shape_name = "Arrow";
      break;
    default:
      break;
    }
    logi("🔷 Shape tool UP - Adding {} with {} points", shape_name, m_temp_stroke.points.size());
    m_document->add_stroke(m_temp_stroke);
  } else {
    logd("Shape tool UP - Not enough points ({} < 2)", m_temp_stroke.points.size());
  }

  // Clear temporary stroke from view
  m_view->clear_current_stroke();
}

void CanvasController::handle_select_tool_down(const nanogui::Vector2f &pos, int modifiers) {
  nanogui::Vector2f global_pos_f = m_view->canvas_to_global(pos);
  nanogui::Vector2i global_pos(static_cast<int>(global_pos_f.x()),
                               static_cast<int>(global_pos_f.y()));

  // Check if clicking resize handle first
  ResizeHandle handle = get_clicked_resize_handle(pos);
  if (handle != ResizeHandle::None) {
    start_resize(pos, handle);
    return;
  }

  // Check if clicking rotation handle
  if (m_view->is_clicking_rotation_handle(global_pos)) {
    // Start rotation mode
    const auto &selected = m_document->get_selected_indices();
    if (!selected.empty()) {
      const auto &strokes = m_document->get_strokes();

      // Calculate rotation center (center of selection bounds)
      float min_x = std::numeric_limits<float>::max();
      float min_y = std::numeric_limits<float>::max();
      float max_x = std::numeric_limits<float>::lowest();
      float max_y = std::numeric_limits<float>::lowest();

      for (int idx : selected) {
        if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
          float s_min_x, s_min_y, s_max_x, s_max_y;
          strokes[idx].get_bounds(s_min_x, s_min_y, s_max_x, s_max_y);
          min_x = std::min(min_x, s_min_x);
          min_y = std::min(min_y, s_min_y);
          max_x = std::max(max_x, s_max_x);
          max_y = std::max(max_y, s_max_y);
        }
      }

      m_rotation_center = nanogui::Vector2f((min_x + max_x) / 2.0f, (min_y + max_y) / 2.0f);

      // Calculate initial angle
      float dx = pos.x() - m_rotation_center.x();
      float dy = pos.y() - m_rotation_center.y();
      m_rotation_start_angle = std::atan2(dy, dx);

      m_mode = Mode::Rotating;
      m_interaction_start = pos;
      return;
    }
  }

  // Check if clicking on an existing stroke
  int clicked_stroke = m_document->find_stroke_at_point(pos.x(), pos.y());

  bool is_shift = (modifiers & 1) != 0; // SHIFT modifier

  if (clicked_stroke >= 0) {
    const auto &strokes = m_document->get_strokes();

    // Check for double-click
    if (is_double_click(pos, clicked_stroke)) {
      logi("=== DOUBLE-CLICK DETECTED on stroke {} ===", clicked_stroke);
      if (clicked_stroke < static_cast<int>(strokes.size())) {
        const Stroke &stroke = strokes[clicked_stroke];
        logi("  Stroke tool: {}, SVGShape tool: {}", static_cast<int>(stroke.tool),
             static_cast<int>(Tool::SVGShape));
        if (stroke.tool == Tool::SVGShape) {
          logi("  -> Calling handle_svg_double_click");
          handle_svg_double_click(clicked_stroke, pos);
          return;
        } else {
          logi("  -> Not an SVG shape, just selecting");
        }
      }

      // For other shapes, just select the individual shape
      m_document->set_selection({clicked_stroke});
      m_mode = Mode::Moving;
      m_interaction_start = pos;
      logi("Double-click: selected individual shape {}", clicked_stroke);
      return;
    }

    // Single click: check if shape is in a group
    int group_id = strokes[clicked_stroke].group_id;

    if (is_shift) {
      // Multi-select: toggle selection
      auto selected = m_document->get_selected_indices();

      if (group_id >= 0) {
        // Toggle entire group
        std::vector<int> group_members = get_group_members(group_id);
        bool group_selected =
            std::find(selected.begin(), selected.end(), clicked_stroke) != selected.end();

        if (group_selected) {
          // Remove all group members
          for (int member : group_members) {
            auto it = std::find(selected.begin(), selected.end(), member);
            if (it != selected.end()) {
              selected.erase(it);
            }
          }
        } else {
          // Add all group members
          for (int member : group_members) {
            if (std::find(selected.begin(), selected.end(), member) == selected.end()) {
              selected.push_back(member);
            }
          }
        }
      } else {
        // Toggle individual shape
        auto it = std::find(selected.begin(), selected.end(), clicked_stroke);
        if (it != selected.end()) {
          selected.erase(it);
        } else {
          selected.push_back(clicked_stroke);
        }
      }
      m_document->set_selection(selected);
    } else {
      // Single select
      if (group_id >= 0) {
        // Select entire group
        std::vector<int> group_members = get_group_members(group_id);
        m_document->set_selection(group_members);
        logi("Selected group {} with {} members", group_id, group_members.size());
      } else {
        // Select individual shape
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

void CanvasController::handle_select_tool_drag(const nanogui::Vector2f &pos, int modifiers) {
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
  } else if (m_mode == Mode::Resizing) {
    // Resize selected strokes
    perform_resize(pos);
  } else if (m_mode == Mode::Rotating) {
    // Rotate selected strokes
    float dx = pos.x() - m_rotation_center.x();
    float dy = pos.y() - m_rotation_center.y();
    float current_angle = std::atan2(dy, dx);
    float angle_delta = current_angle - m_rotation_start_angle;

    // Snap to 15-degree increments if Shift is pressed
    bool is_shift = (modifiers & 1) != 0;
    if (is_shift) {
      float snap_increment = M_PI / 12.0f; // 15 degrees in radians
      angle_delta = std::round(angle_delta / snap_increment) * snap_increment;
    }

    const auto &selected = m_document->get_selected_indices();
    const auto &strokes = m_document->get_strokes();

    for (int idx : selected) {
      if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
        Stroke modified = strokes[idx];
        modified.rotation += angle_delta;
        m_document->update_stroke(idx, modified);
      }
    }

    m_rotation_start_angle = current_angle;
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
  // Get pending shape from document
  std::string shape_id = m_document->get_pending_svg_shape();

  logi("SVG shape tool DOWN at ({:.1f}, {:.1f}), pending shape: '{}'", pos.x(), pos.y(), shape_id);
  if (shape_id.empty() || !m_shape_library) {
    logi("No pending SVG shape or shape library not available");
    return;
  }

  // Just mark that we're ready to place (don't create yet)
  // The shape preview is already shown by draw_pending_shape_preview
  m_mode = Mode::Drawing;
  m_interaction_start = pos;

  logi("Ready to place SVG shape '{}' at mouse release", shape_id);
}

void CanvasController::handle_svg_shape_tool_drag(const nanogui::Vector2f &pos) {
  // For SVG shape placement, we don't need to do anything during drag
  // The preview is handled by draw_pending_shape_preview which follows the cursor
  // Just update interaction position for potential future use
  m_interaction_start = pos;
}

void CanvasController::handle_svg_shape_tool_up(const nanogui::Vector2f &pos) {
  // Get pending shape from document
  std::string shape_id = m_document->get_pending_svg_shape();

  logi("SVG shape tool UP at ({:.1f}, {:.1f}), pending shape: '{}'", pos.x(), pos.y(), shape_id);
  if (shape_id.empty() || !m_shape_library) {
    logi("Cannot place: shape_id empty or no library");
    return;
  }

  // Create shape at release position
  Point snapped = snap_point(Point(pos.x(), pos.y()));
  logi("Creating shape at snapped position ({:.1f}, {:.1f})", snapped.x, snapped.y);
  Stroke stroke = m_shape_library->create_shape(shape_id, snapped);

  if (stroke.svg_data.empty()) {
    loge("Failed to create shape: {}", shape_id);
    m_document->clear_pending_svg_shape();
    m_document->set_current_tool(Tool::Select);
    return;
  }

  // Apply current document properties
  stroke.color = m_document->get_stroke_color();
  stroke.width = m_document->get_stroke_width();

  // Add to document
  m_document->add_stroke(stroke);

  logi("✓ Placed SVG shape '{}' at ({:.1f}, {:.1f})", shape_id, snapped.x, snapped.y);
  m_document->clear_pending_svg_shape();
  m_document->set_current_tool(Tool::Select);

  logi("Switched back to Select tool");
}

// === Helper Methods ===

Point CanvasController::snap_point(const Point &p) {
  if (!m_document->get_snap_enabled()) {
    m_view->clear_snap_indicator();
    return p;
  }

  float snap_distance = m_document->get_snap_distance();
  Point snapped = p;
  float closest_distance = snap_distance;
  bool did_snap = false;

  // Snap to grid
  float grid_size = 20.0f; // Should match GRID_SIZE in CanvasView
  float grid_x = std::round(p.x / grid_size) * grid_size;
  float grid_y = std::round(p.y / grid_size) * grid_size;

  float dx = p.x - grid_x;
  float dy = p.y - grid_y;
  float distance = std::sqrt(dx * dx + dy * dy);

  if (distance < closest_distance) {
    snapped = Point(grid_x, grid_y);
    closest_distance = distance;
    did_snap = true;
  }

  // Check guides for closer snap points
  const auto &guides = m_document->get_guides();
  for (const auto &guide : guides) {
    if (guide.type == Guide::Horizontal) {
      float dist = std::abs(p.y - guide.position);
      if (dist < closest_distance) {
        snapped = Point(p.x, guide.position);
        closest_distance = dist;
        did_snap = true;
      }
    } else if (guide.type == Guide::Vertical) {
      float dist = std::abs(p.x - guide.position);
      if (dist < closest_distance) {
        snapped = Point(guide.position, p.y);
        closest_distance = dist;
        did_snap = true;
      }
    }
  }

  // Update snap indicator
  if (did_snap) {
    m_view->set_snap_indicator(nanogui::Vector2f(snapped.x, snapped.y));
  } else {
    m_view->clear_snap_indicator();
  }

  return snapped;
}

CanvasController::ResizeHandle
CanvasController::get_clicked_resize_handle(const nanogui::Vector2f &pos) {
  const auto &selected = m_document->get_selected_indices();
  if (selected.empty()) {
    return ResizeHandle::None;
  }

  const auto &strokes = m_document->get_strokes();

  // Calculate selection bounds
  float min_x = std::numeric_limits<float>::max();
  float min_y = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float max_y = std::numeric_limits<float>::lowest();

  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      float s_min_x, s_min_y, s_max_x, s_max_y;
      strokes[idx].get_bounds(s_min_x, s_min_y, s_max_x, s_max_y);
      min_x = std::min(min_x, s_min_x);
      min_y = std::min(min_y, s_min_y);
      max_x = std::max(max_x, s_max_x);
      max_y = std::max(max_y, s_max_y);
    }
  }

  if (min_x > max_x) {
    return ResizeHandle::None;
  }

  const float handle_size = 8.0f;
  const float hit_margin = 4.0f; // Extra margin for easier clicking

  // Convert handle positions to canvas coordinates
  nanogui::Vector2f corners[4] = {
      nanogui::Vector2f(min_x, min_y), // Top-left
      nanogui::Vector2f(max_x, min_y), // Top-right
      nanogui::Vector2f(max_x, max_y), // Bottom-right
      nanogui::Vector2f(min_x, max_y)  // Bottom-left
  };

  nanogui::Vector2f edges[4] = {
      nanogui::Vector2f((min_x + max_x) / 2, min_y), // Top
      nanogui::Vector2f(max_x, (min_y + max_y) / 2), // Right
      nanogui::Vector2f((min_x + max_x) / 2, max_y), // Bottom
      nanogui::Vector2f(min_x, (min_y + max_y) / 2)  // Left
  };

  // Check corner handles first (they have priority)
  for (int i = 0; i < 4; i++) {
    if (std::abs(pos.x() - corners[i].x()) <= handle_size / 2 + hit_margin &&
        std::abs(pos.y() - corners[i].y()) <= handle_size / 2 + hit_margin) {
      return static_cast<ResizeHandle>(i);
    }
  }

  // Check edge handles
  for (int i = 0; i < 4; i++) {
    if (std::abs(pos.x() - edges[i].x()) <= handle_size / 2 + hit_margin &&
        std::abs(pos.y() - edges[i].y()) <= handle_size / 2 + hit_margin) {
      return static_cast<ResizeHandle>(i + 4);
    }
  }

  return ResizeHandle::None;
}

void CanvasController::start_resize(const nanogui::Vector2f &pos, ResizeHandle handle) {
  m_mode = Mode::Resizing;
  m_active_resize_handle = handle;
  m_interaction_start = pos;

  const auto &selected = m_document->get_selected_indices();
  const auto &strokes = m_document->get_strokes();

  // Calculate selection bounds
  float min_x = std::numeric_limits<float>::max();
  float min_y = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float max_y = std::numeric_limits<float>::lowest();

  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      float s_min_x, s_min_y, s_max_x, s_max_y;
      strokes[idx].get_bounds(s_min_x, s_min_y, s_max_x, s_max_y);
      min_x = std::min(min_x, s_min_x);
      min_y = std::min(min_y, s_min_y);
      max_x = std::max(max_x, s_max_x);
      max_y = std::max(max_y, s_max_y);
    }
  }

  m_original_width = max_x - min_x;
  m_original_height = max_y - min_y;

  // Set anchor point (opposite corner/edge from the handle being dragged)
  switch (handle) {
  case ResizeHandle::TopLeft:
    m_resize_anchor = nanogui::Vector2f(max_x, max_y);
    break;
  case ResizeHandle::TopRight:
    m_resize_anchor = nanogui::Vector2f(min_x, max_y);
    break;
  case ResizeHandle::BottomRight:
    m_resize_anchor = nanogui::Vector2f(min_x, min_y);
    break;
  case ResizeHandle::BottomLeft:
    m_resize_anchor = nanogui::Vector2f(max_x, min_y);
    break;
  case ResizeHandle::Top:
    m_resize_anchor = nanogui::Vector2f((min_x + max_x) / 2, max_y);
    break;
  case ResizeHandle::Right:
    m_resize_anchor = nanogui::Vector2f(min_x, (min_y + max_y) / 2);
    break;
  case ResizeHandle::Bottom:
    m_resize_anchor = nanogui::Vector2f((min_x + max_x) / 2, min_y);
    break;
  case ResizeHandle::Left:
    m_resize_anchor = nanogui::Vector2f(max_x, (min_y + max_y) / 2);
    break;
  default:
    break;
  }

  logi("Started resize with handle {}, anchor at ({}, {})", static_cast<int>(handle),
       m_resize_anchor.x(), m_resize_anchor.y());
}

void CanvasController::perform_resize(const nanogui::Vector2f &pos) {
  const auto &selected = m_document->get_selected_indices();
  const auto &strokes = m_document->get_strokes();

  if (selected.empty() || m_original_width <= 0 || m_original_height <= 0) {
    return;
  }

  // Calculate new dimensions based on handle being dragged
  float new_width = m_original_width;
  float new_height = m_original_height;

  switch (m_active_resize_handle) {
  case ResizeHandle::TopLeft:
  case ResizeHandle::TopRight:
  case ResizeHandle::BottomLeft:
  case ResizeHandle::BottomRight: {
    // Corner handles - resize both dimensions
    float dx = std::abs(pos.x() - m_resize_anchor.x());
    float dy = std::abs(pos.y() - m_resize_anchor.y());
    new_width = dx;
    new_height = dy;
    break;
  }
  case ResizeHandle::Top:
  case ResizeHandle::Bottom:
    // Vertical edge handles - resize height only
    new_height = std::abs(pos.y() - m_resize_anchor.y());
    break;
  case ResizeHandle::Left:
  case ResizeHandle::Right:
    // Horizontal edge handles - resize width only
    new_width = std::abs(pos.x() - m_resize_anchor.x());
    break;
  default:
    return;
  }

  // Prevent zero or negative dimensions
  new_width = std::max(new_width, 10.0f);
  new_height = std::max(new_height, 10.0f);

  // Calculate scale factors
  float scale_x = new_width / m_original_width;
  float scale_y = new_height / m_original_height;

  // Apply resize to all selected strokes
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      Stroke modified = strokes[idx];

      // For SVG shapes, update the scale factors
      if (modified.tool == Tool::SVGShape) {
        modified.svg_scale_x *= scale_x;
        modified.svg_scale_y *= scale_y;

        // Update position to maintain anchor point
        if (!modified.points.empty()) {
          float old_x = modified.points[0].x;
          float old_y = modified.points[0].y;

          // Calculate new position relative to anchor
          float rel_x = old_x - m_resize_anchor.x();
          float rel_y = old_y - m_resize_anchor.y();

          modified.points[0].x = m_resize_anchor.x() + rel_x * scale_x;
          modified.points[0].y = m_resize_anchor.y() + rel_y * scale_y;
        }
      }

      m_document->update_stroke(idx, modified);
    }
  }

  // Update for next frame
  m_original_width = new_width;
  m_original_height = new_height;
}

bool CanvasController::is_clicking_selection_handle(const nanogui::Vector2f &pos) {
  return get_clicked_resize_handle(pos) != ResizeHandle::None;
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
  bool is_double = (elapsed.count() < 500) && (m_last_clicked_stroke == stroke_index) &&
                   (std::abs(pos.x() - m_last_click_pos.x()) < 5.0f) &&
                   (std::abs(pos.y() - m_last_click_pos.y()) < 5.0f);

  logd("is_double_click: elapsed={}ms, same_stroke={}, pos_delta=({}, {}), result={}",
       elapsed.count(), m_last_clicked_stroke == stroke_index,
       std::abs(pos.x() - m_last_click_pos.x()), std::abs(pos.y() - m_last_click_pos.y()),
       is_double);
  m_last_click_time = now;
  m_last_click_pos = pos;
  m_last_clicked_stroke = stroke_index;

  return is_double;
}

void CanvasController::handle_svg_double_click(int stroke_index, const nanogui::Vector2f &pos) {
  logi("=== Double-click detected on SVG shape at index {} ===", stroke_index);
  const auto &strokes = m_document->get_strokes();
  if (stroke_index < 0 || stroke_index >= static_cast<int>(strokes.size())) {
    loge("Invalid stroke index: {}", stroke_index);
    return;
  }

  const Stroke &stroke = strokes[stroke_index];
  logi("Stroke shape_id: '{}', tool: {}", stroke.svg_shape_id, static_cast<int>(stroke.tool));
  if (stroke.svg_shape_id.empty()) {
    logw("SVG shape has no shape_id, cannot edit parameters");
    return;
  }

  if (!m_shape_library) {
    logw("No shape library available for editing");
    return;
  }

  // Get shape definition to find text parameters
  const ShapeDefinition *shape = m_shape_library->get_shape(stroke.svg_shape_id);
  if (!shape) {
    logw("Shape definition not found for: {}", stroke.svg_shape_id);
    return;
  }

  logi("Found shape definition: '{}' with {} parameters", shape->name, shape->parameters.size());
  // Get the stroke bounds to calculate relative click position
  float min_x, min_y, max_x, max_y;
  stroke.get_bounds(min_x, min_y, max_x, max_y);
  float shape_height = max_y - min_y;
  float relative_y = (pos.y() - min_y) / shape_height;

  logi("  Click position: relative_y={:.2f} (0=top, 1=bottom)", relative_y);
  std::string text_param_name;

  if (stroke.svg_shape_id == "uml.class") {
    // UML Class: className (top 0-0.27), attributes (middle 0.27-0.6), methods (bottom 0.6-1.0)
    if (relative_y < 0.27f) {
      text_param_name = "className";
    } else if (relative_y < 0.6f) {
      text_param_name = "attributes";
    } else {
      text_param_name = "methods";
    }
  } else if (stroke.svg_shape_id == "uml.interface") {
    // UML Interface: interfaceName (top 0-0.33), methods (bottom 0.33-1.0)
    if (relative_y < 0.33f) {
      text_param_name = "interfaceName";
    } else {
      text_param_name = "methods";
    }
  } else {
    // Default: use first text parameter
    for (const auto &param : shape->parameters) {
      if (param.type == "text") {
        text_param_name = param.name;
        break;
      }
    }
  }

  logi("  Selected parameter: '{}'", text_param_name);
  if (!text_param_name.empty()) {
    // For multi-line parameters (methods/attributes), detect which line was clicked
    bool is_multi_line = (text_param_name == "methods" || text_param_name == "attributes");
    int line_index = 0;
    std::string line_to_edit;

    if (is_multi_line) {
      // Get the current value and split into lines
      auto it = stroke.svg_parameters.find(text_param_name);
      if (it != stroke.svg_parameters.end()) {
        std::string full_text = it->second;
        std::vector<std::string> lines;
        std::stringstream ss(full_text);
        std::string line;
        while (std::getline(ss, line)) {
          lines.push_back(line);
        }

        if (!lines.empty()) {
          // Calculate which line was clicked based on relative position within the section
          float section_start_y, section_end_y;
          if (text_param_name == "attributes") {
            section_start_y = 0.27f;
            section_end_y = 0.6f;
          } else { // methods
            section_start_y = 0.6f;
            section_end_y = 1.0f;
          }

          float relative_y_in_section = (relative_y - section_start_y) / (section_end_y - section_start_y);
          line_index = static_cast<int>(relative_y_in_section * lines.size());
          line_index = std::max(0, std::min(line_index, static_cast<int>(lines.size()) - 1));
          line_to_edit = lines[line_index];

          logi("  Multi-line field: {} lines, clicked line {}: '{}'", lines.size(), line_index, line_to_edit);
        }
      }
    }

    logi("✓ Opening inline editor for parameter: '{}'{}", text_param_name, 
         is_multi_line ? " (line " + std::to_string(line_index) + ")" : "");
    try {
      // Calculate screen position - place editor near the click position
      nanogui::Vector2f screen_pos = m_view->canvas_to_global(pos);

      // Convert to integer position and offset slightly to the right
      nanogui::Vector2i editor_pos(static_cast<int>(screen_pos.x()) + 10,
                                   static_cast<int>(screen_pos.y()) - 15);

      logi("  Click pos: ({}, {}), Screen pos: ({}, {}), Editor pos: ({}, {})", 
           pos.x(), pos.y(), screen_pos.x(), screen_pos.y(), editor_pos.x(), editor_pos.y());
      nanogui::Widget *parent = m_view;
      while (parent->parent() != nullptr) {
        parent = parent->parent();
      }

      logi("  Creating editor with parent: {}", (void *)parent);
      auto *editor = new InlineTextEditor(parent, m_document, m_shape_library, stroke_index,
                                          text_param_name, editor_pos, line_index);

      logi("  InlineTextEditor created successfully at ({}, {})", editor->position().x(),
           editor->position().y());
      editor->activate();
      editor->set_visible(true);

      if (parent->screen()) {
        parent->screen()->perform_layout();
        parent->screen()->redraw();
      }

      editor->request_focus();

      logi("  InlineTextEditor activated, focused, and screen redrawn");
      logi("  Editor visible: {}, size: {}x{}, focused: {}", editor->visible(), editor->width(),
           editor->height(), editor->focused());
    } catch (const std::exception &e) {
      loge("Exception creating inline editor: {}", e.what());
    }
  } else {
    // No editable text parameters - use full parameter editor dialog
    logi("No className/interfaceName found, opening full parameter editor");
    auto *editor =
        new SVGParameterEditor(m_view->parent(), m_document, m_shape_library, stroke_index);
    editor->show();
  }

  logi("=== handle_svg_double_click complete ===");
}

void CanvasController::start_guide_creation(Guide::Type type, const nanogui::Vector2f &pos) {
  m_mode = Mode::CreatingGuide;
  m_guide_type = type;
  m_interaction_start = pos;
  m_dragging_guide_index = -1; // New guide, not dragging existing one

  // Set initial guide position
  if (type == Guide::Horizontal) {
    m_temp_guide_position = pos.y();
  } else {
    m_temp_guide_position = pos.x();
  }

  m_view->set_temp_guide(type, m_temp_guide_position);
}

int CanvasController::find_guide_at_point(const nanogui::Vector2f &pos) const {
  const auto &guides = m_document->get_guides();
  float threshold = 5.0f; // 5 pixels threshold for clicking on guide

  for (size_t i = 0; i < guides.size(); ++i) {
    const auto &guide = guides[i];
    if (!guide.visible) {
      continue;
    }

    if (guide.type == Guide::Horizontal) {
      // Check if clicking near horizontal guide
      if (std::abs(pos.y() - guide.position) < threshold) {
        return static_cast<int>(i);
      }
    } else {
      // Check if clicking near vertical guide
      if (std::abs(pos.x() - guide.position) < threshold) {
        return static_cast<int>(i);
      }
    }
  }

  return -1; // No guide found
}

// === Alignment Operations ===

void CanvasController::align_selection_left() {
  auto selected = m_document->get_selected_indices();
  if (selected.size() < 2) {
    return; // Need at least 2 shapes to align
  }

  auto strokes = m_document->get_strokes();

  // Find leftmost edge
  float min_x = std::numeric_limits<float>::max();
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      float x1, y1, x2, y2;
      strokes[idx].get_bounds(x1, y1, x2, y2);
      min_x = std::min(min_x, x1);
    }
  }

  // Align all shapes to leftmost edge
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      Stroke updated = strokes[idx];
      float x1, y1, x2, y2;
      updated.get_bounds(x1, y1, x2, y2);
      float offset = min_x - x1;

      // Move all points
      for (auto &pt : updated.points) {
        pt.x += offset;
      }

      m_document->update_stroke(idx, updated);
    }
  }
}

void CanvasController::align_selection_right() {
  auto selected = m_document->get_selected_indices();
  if (selected.size() < 2) {
    return;
  }

  auto strokes = m_document->get_strokes();

  // Find rightmost edge
  float max_x = std::numeric_limits<float>::lowest();
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      float x1, y1, x2, y2;
      strokes[idx].get_bounds(x1, y1, x2, y2);
      max_x = std::max(max_x, x2);
    }
  }

  // Align all shapes to rightmost edge
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      Stroke updated = strokes[idx];
      float x1, y1, x2, y2;
      updated.get_bounds(x1, y1, x2, y2);
      float offset = max_x - x2;

      // Move all points
      for (auto &pt : updated.points) {
        pt.x += offset;
      }

      m_document->update_stroke(idx, updated);
    }
  }
}

void CanvasController::align_selection_top() {
  auto selected = m_document->get_selected_indices();
  if (selected.size() < 2) {
    return;
  }

  auto strokes = m_document->get_strokes();

  // Find topmost edge
  float min_y = std::numeric_limits<float>::max();
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      float x1, y1, x2, y2;
      strokes[idx].get_bounds(x1, y1, x2, y2);
      min_y = std::min(min_y, y1);
    }
  }

  // Align all shapes to topmost edge
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      Stroke updated = strokes[idx];
      float x1, y1, x2, y2;
      updated.get_bounds(x1, y1, x2, y2);
      float offset = min_y - y1;

      // Move all points
      for (auto &pt : updated.points) {
        pt.y += offset;
      }

      m_document->update_stroke(idx, updated);
    }
  }
}

void CanvasController::align_selection_bottom() {
  logi("Align to bottom clicked");
  auto selected = m_document->get_selected_indices();
  logi("   Selected strokes: {}", selected.size());
  if (selected.size() < 2) {
    logw("   Not enough strokes selected (need >= 2)");
    return;
  }

  auto strokes = m_document->get_strokes();
  logi("   Total strokes in document: {}", strokes.size());
  float max_y = std::numeric_limits<float>::lowest();
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      float x1, y1, x2, y2;
      strokes[idx].get_bounds(x1, y1, x2, y2);
      logi("   Stroke {} bounds: y1={:.1f}, y2={:.1f}", idx, y1, y2);
      max_y = std::max(max_y, y2);
    }
  }

  logi("   Bottommost edge: {:.1f}", max_y);
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      logi("   Processing stroke {}", idx);
      Stroke updated = strokes[idx];
      float x1, y1, x2, y2;
      updated.get_bounds(x1, y1, x2, y2);
      float offset = max_y - y2;

      logi("   Offset for stroke {}: {:.1f}", idx, offset);
      for (auto &pt : updated.points) {
        pt.y += offset;
      }

      logi("   Updating stroke {} in document", idx);
      m_document->update_stroke(idx, updated);

      logi("   Stroke {} updated", idx);
    }
  }

  logi("Align to bottom complete");
}

void CanvasController::align_selection_center_horizontal() {
  auto selected = m_document->get_selected_indices();
  if (selected.size() < 2) {
    return;
  }

  auto strokes = m_document->get_strokes();

  // Calculate average center X
  float sum_center_x = 0.0f;
  int count = 0;
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      float x1, y1, x2, y2;
      strokes[idx].get_bounds(x1, y1, x2, y2);
      sum_center_x += (x1 + x2) / 2.0f;
      count++;
    }
  }

  if (count == 0)
    return;
  float target_center_x = sum_center_x / count;

  // Align all shapes to average center
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      Stroke updated = strokes[idx];
      float x1, y1, x2, y2;
      updated.get_bounds(x1, y1, x2, y2);
      float current_center_x = (x1 + x2) / 2.0f;
      float offset = target_center_x - current_center_x;

      // Move all points
      for (auto &pt : updated.points) {
        pt.x += offset;
      }

      m_document->update_stroke(idx, updated);
    }
  }
}

void CanvasController::align_selection_center_vertical() {
  auto selected = m_document->get_selected_indices();
  if (selected.size() < 2) {
    return;
  }

  auto strokes = m_document->get_strokes();

  // Calculate average center Y
  float sum_center_y = 0.0f;
  int count = 0;
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      float x1, y1, x2, y2;
      strokes[idx].get_bounds(x1, y1, x2, y2);
      sum_center_y += (y1 + y2) / 2.0f;
      count++;
    }
  }

  if (count == 0)
    return;
  float target_center_y = sum_center_y / count;

  // Align all shapes to average center
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      Stroke updated = strokes[idx];
      float x1, y1, x2, y2;
      updated.get_bounds(x1, y1, x2, y2);
      float current_center_y = (y1 + y2) / 2.0f;
      float offset = target_center_y - current_center_y;

      // Move all points
      for (auto &pt : updated.points) {
        pt.y += offset;
      }

      m_document->update_stroke(idx, updated);
    }
  }
}

// === Shape Operations ===

void CanvasController::group_selected_shapes() {
  const auto &selected = m_document->get_selected_indices();

  if (selected.size() < 2) {
    logi("Cannot group: need at least 2 shapes selected");
    return;
  }

  // Find next available group ID
  const auto &strokes = m_document->get_strokes();
  int max_group_id = 0;
  for (const auto &stroke : strokes) {
    if (stroke.group_id > max_group_id) {
      max_group_id = stroke.group_id;
    }
  }
  int new_group_id = max_group_id + 1;

  // Assign group ID to all selected shapes
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      Stroke modified = strokes[idx];
      modified.group_id = new_group_id;
      m_document->update_stroke(idx, modified);
    }
  }

  logi("✓ Grouped {} shapes with group ID {}", selected.size(), new_group_id);
}

void CanvasController::ungroup_selected_shapes() {
  const auto &selected = m_document->get_selected_indices();
  const auto &strokes = m_document->get_strokes();

  if (selected.empty()) {
    logi("Cannot ungroup: no shapes selected");
    return;
  }

  int ungrouped_count = 0;
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      if (strokes[idx].group_id >= 0) {
        Stroke modified = strokes[idx];
        modified.group_id = -1;
        m_document->update_stroke(idx, modified);
        ungrouped_count++;
      }
    }
  }

  logi("✓ Ungrouped {} shapes", ungrouped_count);
}

void CanvasController::duplicate_selected_shapes() {
  const auto &selected = m_document->get_selected_indices();
  const auto &strokes = m_document->get_strokes();

  if (selected.empty()) {
    logi("Cannot duplicate: no shapes selected");
    return;
  }

  std::vector<int> new_indices;

  // Duplicate each selected shape with a small offset
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      Stroke duplicate = strokes[idx];

      // Offset by 20 pixels
      duplicate.move(20.0f, 20.0f);

      // Add to document
      m_document->add_stroke(duplicate);
      new_indices.push_back(static_cast<int>(m_document->get_strokes().size()) - 1);
    }
  }

  // Select the duplicated shapes
  m_document->set_selection(new_indices);

  logi("✓ Duplicated {} shapes", new_indices.size());
}

void CanvasController::delete_selected_shapes() {
  const auto &selected = m_document->get_selected_indices();

  if (selected.empty()) {
    logi("Cannot delete: no shapes selected");
    return;
  }

  int count = selected.size();
  m_document->remove_strokes(selected);

  logi("✓ Deleted {} shapes", count);
}

std::vector<int> CanvasController::get_group_members(int group_id) const {
  std::vector<int> members;
  const auto &strokes = m_document->get_strokes();

  for (size_t i = 0; i < strokes.size(); ++i) {
    if (strokes[i].group_id == group_id) {
      members.push_back(static_cast<int>(i));
    }
  }

  return members;
}

void CanvasController::copy_selected_shapes() {
  const auto &selected = m_document->get_selected_indices();

  if (selected.empty()) {
    logi("Cannot copy: no shapes selected");
    return;
  }

  const auto &strokes = m_document->get_strokes();
  m_clipboard.clear();

  // Copy selected shapes to clipboard
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      m_clipboard.push_back(strokes[idx]);
    }
  }

  logi("✓ Copied {} shapes to clipboard", m_clipboard.size());
}

void CanvasController::cut_selected_shapes() {
  const auto &selected = m_document->get_selected_indices();

  if (selected.empty()) {
    logi("Cannot cut: no shapes selected");
    return;
  }

  // Copy first
  copy_selected_shapes();

  // Then delete
  m_document->remove_strokes(selected);

  logi("✓ Cut {} shapes", m_clipboard.size());
}

void CanvasController::paste_shapes() {
  if (m_clipboard.empty()) {
    logi("Cannot paste: clipboard is empty");
    return;
  }

  std::vector<int> new_indices;

  // Paste shapes with offset
  for (const auto &stroke : m_clipboard) {
    Stroke pasted = stroke;

    // Offset by 20 pixels so pasted shapes don't overlap originals
    pasted.move(20.0f, 20.0f);

    // Add to document
    m_document->add_stroke(pasted);
    new_indices.push_back(static_cast<int>(m_document->get_strokes().size()) - 1);
  }

  // Select the pasted shapes
  m_document->set_selection(new_indices);

  logi("✓ Pasted {} shapes", new_indices.size());
}

void CanvasController::nudge_selection(float dx, float dy) {
  const auto &selected = m_document->get_selected_indices();

  if (selected.empty()) {
    return;
  }

  const auto &strokes = m_document->get_strokes();

  // Move all selected shapes
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      Stroke modified = strokes[idx];
      modified.move(dx, dy);
      m_document->update_stroke(idx, modified);
    }
  }

  logd("Nudged {} shapes by ({}, {})", selected.size(), dx, dy);
}

void CanvasController::export_to_png() {
  // Generate filename with timestamp
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << "whiteboard_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".png";

  // PNG export requires NVGcontext from the screen
  // For now, log that this feature requires integration at the app level
  logi("PNG export requested: {}", ss.str());
  logi("Note: PNG export requires NVGcontext from the Screen/App level");
  logi("Please use ExportManager::export_to_png() from the app with nvg_context()");
}

void CanvasController::export_to_svg() {
  // Generate filename with timestamp
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << "whiteboard_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".svg";

  if (m_document->export_to_svg(ss.str())) {
    logi("✓ Exported to SVG: {}", ss.str());
  } else {
    loge("✗ Failed to export SVG: {}", ss.str());
  }
}

} // namespace whiteboard

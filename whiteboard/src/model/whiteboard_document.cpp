/**
 * \file whiteboard_document.cpp
 * \brief Implementation of WhiteboardDocument class.
 */

#include "whiteboard/model/whiteboard_document.h"
#include "whiteboard/serialization.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <chrono>
#include <nanovg.h>

namespace whiteboard {

WhiteboardDocument::WhiteboardDocument()
    : m_current_tool(Tool::Pen), m_stroke_color(255, 100, 100, 255), m_stroke_width(3.0f),
      m_fill_style(FillStyle::None), m_fill_color(255, 182, 193, 255), m_font_face("sans"),
      m_font_size(16.0f), m_text_align(NVG_ALIGN_LEFT | NVG_ALIGN_TOP), m_zoom(1.0f),
      m_pan_offset(0.0f, 0.0f), m_grid_visible(false), m_guides_visible(false),
      m_snap_enabled(false) {}

// === Helper Methods ===

void WhiteboardDocument::push_undo_state() {
  m_undo_stack.push(capture_state());
  // Clear redo stack when new action is performed
  while (!m_redo_stack.empty()) {
    m_redo_stack.pop();
  }
}

DocumentState WhiteboardDocument::capture_state() const {
  return DocumentState(m_strokes, m_selected_indices, m_guides);
}

void WhiteboardDocument::restore_state(const DocumentState &state) {
  m_strokes = state.strokes;
  m_selected_indices = state.selected_indices;
  m_guides = state.guides;
}

// === Notification Methods ===

void WhiteboardDocument::notify_strokes_changed() {
  for (auto *observer : m_observers) {
    try {
      observer->on_strokes_changed();
    } catch (const std::exception &e) {
      std::cerr << "Observer error in on_strokes_changed: " << e.what() << std::endl;
    }
  }
}

void WhiteboardDocument::notify_selection_changed() {
  for (auto *observer : m_observers) {
    try {
      observer->on_selection_changed();
    } catch (const std::exception &e) {
      std::cerr << "Observer error in on_selection_changed: " << e.what() << std::endl;
    }
  }
}

void WhiteboardDocument::notify_tool_changed() {
  for (auto *observer : m_observers) {
    try {
      observer->on_tool_changed();
    } catch (const std::exception &e) {
      std::cerr << "Observer error in on_tool_changed: " << e.what() << std::endl;
    }
  }
}

void WhiteboardDocument::notify_properties_changed() {
  for (auto *observer : m_observers) {
    try {
      observer->on_properties_changed();
    } catch (const std::exception &e) {
      std::cerr << "Observer error in on_properties_changed: " << e.what() << std::endl;
    }
  }
}

void WhiteboardDocument::notify_view_changed() {
  for (auto *observer : m_observers) {
    try {
      observer->on_view_changed();
    } catch (const std::exception &e) {
      std::cerr << "Observer error in on_view_changed: " << e.what() << std::endl;
    }
  }
}

// === Observer Management ===

void WhiteboardDocument::add_observer(IDocumentObserver *observer) {
  if (observer && std::find(m_observers.begin(), m_observers.end(), observer) == m_observers.end()) {
    m_observers.push_back(observer);
  }
}

void WhiteboardDocument::remove_observer(IDocumentObserver *observer) {
  m_observers.erase(std::remove(m_observers.begin(), m_observers.end(), observer), m_observers.end());
}

// Placeholder implementations for remaining methods
// These will be implemented in subsequent tasks

void WhiteboardDocument::add_stroke(const Stroke &stroke) {
  push_undo_state();
  m_strokes.push_back(stroke);
  notify_strokes_changed();
}

void WhiteboardDocument::remove_strokes(const std::vector<int> &indices) {
  if (indices.empty()) {
    return;
  }

  // Validate indices
  for (int idx : indices) {
    if (idx < 0 || idx >= static_cast<int>(m_strokes.size())) {
      std::cerr << "Invalid stroke index: " << idx << std::endl;
      return;
    }
  }

  push_undo_state();

  // Sort indices in descending order to remove from back to front
  std::vector<int> sorted_indices = indices;
  std::sort(sorted_indices.begin(), sorted_indices.end(), std::greater<int>());

  // Remove strokes
  for (int idx : sorted_indices) {
    m_strokes.erase(m_strokes.begin() + idx);
  }

  // Update selection to remove deleted indices
  std::vector<int> new_selection;
  for (int sel_idx : m_selected_indices) {
    bool was_deleted = std::find(indices.begin(), indices.end(), sel_idx) != indices.end();
    if (!was_deleted) {
      // Adjust index based on how many strokes before it were deleted
      int adjustment = 0;
      for (int del_idx : indices) {
        if (del_idx < sel_idx) {
          adjustment++;
        }
      }
      new_selection.push_back(sel_idx - adjustment);
    }
  }
  m_selected_indices = new_selection;

  notify_strokes_changed();
  if (new_selection.size() != indices.size()) {
    notify_selection_changed();
  }
}

void WhiteboardDocument::update_stroke(int index, const Stroke &stroke) {
  if (index < 0 || index >= static_cast<int>(m_strokes.size())) {
    std::cerr << "Invalid stroke index: " << index << std::endl;
    return;
  }

  push_undo_state();
  m_strokes[index] = stroke;
  notify_strokes_changed();
}

int WhiteboardDocument::find_stroke_at_point(float x, float y, float margin) const {
  // Search from back to front (top to bottom in z-order)
  for (int i = static_cast<int>(m_strokes.size()) - 1; i >= 0; --i) {
    const auto &stroke = m_strokes[i];
    if (!stroke.visible || stroke.locked) {
      continue;
    }
    if (stroke.contains_point(x, y, margin)) {
      return i;
    }
  }
  return -1;
}

std::vector<int> WhiteboardDocument::find_strokes_in_rect(float x1, float y1, float x2,
                                                           float y2) const {
  std::vector<int> result;

  // Normalize rectangle
  float min_x = std::min(x1, x2);
  float max_x = std::max(x1, x2);
  float min_y = std::min(y1, y2);
  float max_y = std::max(y1, y2);

  for (size_t i = 0; i < m_strokes.size(); ++i) {
    const auto &stroke = m_strokes[i];
    if (!stroke.visible || stroke.locked) {
      continue;
    }

    // Get stroke bounds
    float stroke_min_x, stroke_min_y, stroke_max_x, stroke_max_y;
    stroke.get_bounds(stroke_min_x, stroke_min_y, stroke_max_x, stroke_max_y);

    // Check if stroke bounds intersect with selection rectangle
    if (stroke_max_x >= min_x && stroke_min_x <= max_x && stroke_max_y >= min_y &&
        stroke_min_y <= max_y) {
      result.push_back(static_cast<int>(i));
    }
  }

  return result;
}

void WhiteboardDocument::set_selection(const std::vector<int> &indices) {
  // Validate indices
  std::vector<int> valid_indices;
  for (int idx : indices) {
    if (idx >= 0 && idx < static_cast<int>(m_strokes.size())) {
      valid_indices.push_back(idx);
    }
  }

  m_selected_indices = valid_indices;
  notify_selection_changed();
}

void WhiteboardDocument::clear_selection() {
  if (!m_selected_indices.empty()) {
    m_selected_indices.clear();
    notify_selection_changed();
  }
}

bool WhiteboardDocument::is_selected(int index) const {
  return std::find(m_selected_indices.begin(), m_selected_indices.end(), index) !=
         m_selected_indices.end();
}

void WhiteboardDocument::set_current_tool(Tool tool) {
  if (m_current_tool != tool) {
    m_current_tool = tool;
    notify_tool_changed();
  }
}

void WhiteboardDocument::set_stroke_color(const nanogui::Color &color) {
  m_stroke_color = color;
  notify_properties_changed();
}

void WhiteboardDocument::set_stroke_width(float width) {
  if (width > 0.0f) {
    m_stroke_width = width;
    notify_properties_changed();
  }
}

void WhiteboardDocument::set_fill_style(FillStyle style) {
  m_fill_style = style;
  notify_properties_changed();
}

void WhiteboardDocument::set_fill_color(const nanogui::Color &color) {
  m_fill_color = color;
  notify_properties_changed();
}

void WhiteboardDocument::set_font_face(const std::string &face) {
  m_font_face = face;
  notify_properties_changed();
}

void WhiteboardDocument::set_font_size(float size) {
  if (size > 0.0f) {
    m_font_size = size;
    notify_properties_changed();
  }
}

void WhiteboardDocument::set_text_align(int align) {
  m_text_align = align;
  notify_properties_changed();
}

void WhiteboardDocument::set_zoom(float zoom) {
  // Clamp zoom to reasonable range
  float clamped_zoom = std::max(0.25f, std::min(6.0f, zoom));
  if (std::abs(m_zoom - clamped_zoom) > 1e-6f) {
    m_zoom = clamped_zoom;
    notify_view_changed();
  }
}

void WhiteboardDocument::set_pan_offset(const nanogui::Vector2f &offset) {
  m_pan_offset = offset;
  notify_view_changed();
}

void WhiteboardDocument::set_grid_visible(bool visible) {
  if (m_grid_visible != visible) {
    m_grid_visible = visible;
    notify_view_changed();
  }
}

void WhiteboardDocument::set_guides_visible(bool visible) {
  if (m_guides_visible != visible) {
    m_guides_visible = visible;
    notify_view_changed();
  }
}

void WhiteboardDocument::set_snap_enabled(bool enabled) {
  if (m_snap_enabled != enabled) {
    m_snap_enabled = enabled;
    notify_view_changed();
  }
}

void WhiteboardDocument::add_guide(const Guide &guide) {
  m_guides.push_back(guide);
  notify_strokes_changed(); // Guides affect rendering, so notify as strokes changed
}

void WhiteboardDocument::remove_guide(int index) {
  if (index >= 0 && index < static_cast<int>(m_guides.size())) {
    m_guides.erase(m_guides.begin() + index);
    notify_strokes_changed();
  }
}

void WhiteboardDocument::undo() {
  if (m_undo_stack.empty()) {
    return;
  }

  // Push current state to redo stack
  m_redo_stack.push(capture_state());

  // Restore state from undo stack
  DocumentState state = m_undo_stack.top();
  m_undo_stack.pop();
  restore_state(state);

  // Notify observers
  notify_strokes_changed();
  notify_selection_changed();
}

void WhiteboardDocument::redo() {
  if (m_redo_stack.empty()) {
    return;
  }

  // Push current state to undo stack
  m_undo_stack.push(capture_state());

  // Restore state from redo stack
  DocumentState state = m_redo_stack.top();
  m_redo_stack.pop();
  restore_state(state);

  // Notify observers
  notify_strokes_changed();
  notify_selection_changed();
}

void WhiteboardDocument::set_stroke_visible(int index, bool visible) {
  if (index >= 0 && index < static_cast<int>(m_strokes.size())) {
    if (m_strokes[index].visible != visible) {
      m_strokes[index].visible = visible;
      notify_strokes_changed();
    }
  }
}

void WhiteboardDocument::set_stroke_locked(int index, bool locked) {
  if (index >= 0 && index < static_cast<int>(m_strokes.size())) {
    if (m_strokes[index].locked != locked) {
      m_strokes[index].locked = locked;
      notify_strokes_changed();
    }
  }
}

void WhiteboardDocument::set_stroke_name(int index, const std::string &name) {
  if (index >= 0 && index < static_cast<int>(m_strokes.size())) {
    m_strokes[index].name = name;
    notify_strokes_changed();
  }
}

void WhiteboardDocument::reorder_stroke(int from_index, int to_index) {
  if (from_index < 0 || from_index >= static_cast<int>(m_strokes.size()) || to_index < 0 ||
      to_index >= static_cast<int>(m_strokes.size()) || from_index == to_index) {
    return;
  }

  push_undo_state();

  // Move stroke from from_index to to_index
  Stroke stroke = m_strokes[from_index];
  m_strokes.erase(m_strokes.begin() + from_index);
  m_strokes.insert(m_strokes.begin() + to_index, stroke);

  // Update selection indices
  std::vector<int> new_selection;
  for (int sel_idx : m_selected_indices) {
    if (sel_idx == from_index) {
      new_selection.push_back(to_index);
    } else if (from_index < to_index) {
      // Moving forward: indices between from and to shift back
      if (sel_idx > from_index && sel_idx <= to_index) {
        new_selection.push_back(sel_idx - 1);
      } else {
        new_selection.push_back(sel_idx);
      }
    } else {
      // Moving backward: indices between to and from shift forward
      if (sel_idx >= to_index && sel_idx < from_index) {
        new_selection.push_back(sel_idx + 1);
      } else {
        new_selection.push_back(sel_idx);
      }
    }
  }
  m_selected_indices = new_selection;

  notify_strokes_changed();
}

// === Serialization ===

nlohmann::json WhiteboardDocument::to_json() const {
  nlohmann::json j;
  
  // Metadata
  j["version"] = "1.0";
  j["created"] = std::chrono::system_clock::now().time_since_epoch().count();
  
  // Canvas state
  j["zoom"] = m_zoom;
  j["pan_offset"] = {m_pan_offset.x(), m_pan_offset.y()};
  j["grid_visible"] = m_grid_visible;
  j["guides_visible"] = m_guides_visible;
  j["snap_enabled"] = m_snap_enabled;
  
  // Tool state
  j["current_tool"] = static_cast<int>(m_current_tool);
  j["stroke_color"] = color_to_json(m_stroke_color);
  j["stroke_width"] = m_stroke_width;
  j["fill_style"] = static_cast<int>(m_fill_style);
  j["fill_color"] = color_to_json(m_fill_color);
  
  // Text properties
  j["font_face"] = m_font_face;
  j["font_size"] = m_font_size;
  j["text_align"] = m_text_align;
  
  // Guides
  j["guides"] = nlohmann::json::array();
  for (const auto& guide : m_guides) {
    nlohmann::json g;
    g["type"] = static_cast<int>(guide.type);
    g["position"] = guide.position;
    g["visible"] = guide.visible;
    j["guides"].push_back(g);
  }
  
  // Strokes
  j["strokes"] = nlohmann::json::array();
  for (const auto& stroke : m_strokes) {
    j["strokes"].push_back(stroke_to_json(stroke));
  }
  
  // Note: Selection is not saved (runtime state)
  
  return j;
}

void WhiteboardDocument::from_json(const nlohmann::json& j) {
  // Clear current state
  m_strokes.clear();
  m_guides.clear();
  m_selected_indices.clear();
  
  // Clear undo/redo stacks
  while (!m_undo_stack.empty()) m_undo_stack.pop();
  while (!m_redo_stack.empty()) m_redo_stack.pop();
  
  // Canvas state
  m_zoom = j.value("zoom", 1.0f);
  auto pan = j.value("pan_offset", std::vector<float>{0, 0});
  m_pan_offset = nanogui::Vector2f(pan[0], pan[1]);
  m_grid_visible = j.value("grid_visible", true);
  m_guides_visible = j.value("guides_visible", false);
  m_snap_enabled = j.value("snap_enabled", false);
  
  // Tool state
  m_current_tool = static_cast<Tool>(j.value("current_tool", static_cast<int>(Tool::Pen)));
  if (j.contains("stroke_color")) {
    m_stroke_color = color_from_json(j["stroke_color"]);
  }
  m_stroke_width = j.value("stroke_width", 3.0f);
  m_fill_style = static_cast<FillStyle>(j.value("fill_style", static_cast<int>(FillStyle::None)));
  if (j.contains("fill_color")) {
    m_fill_color = color_from_json(j["fill_color"]);
  }
  
  // Text properties
  m_font_face = j.value("font_face", "sans");
  m_font_size = j.value("font_size", 16.0f);
  m_text_align = j.value("text_align", 0);
  
  // Guides
  if (j.contains("guides")) {
    for (const auto& g : j["guides"]) {
      Guide guide(
        static_cast<Guide::Type>(g["type"].get<int>()),
        g["position"].get<float>()
      );
      guide.visible = g.value("visible", true);
      m_guides.push_back(guide);
    }
  }
  
  // Strokes
  if (j.contains("strokes")) {
    for (const auto& s : j["strokes"]) {
      m_strokes.push_back(stroke_from_json(s));
    }
  }
  
  // Notify observers
  notify_strokes_changed();
  notify_view_changed();
  notify_tool_changed();
}

bool WhiteboardDocument::save_to_file(const std::string& filename) {
  try {
    nlohmann::json j = to_json();
    std::ofstream file(filename);
    if (!file.is_open()) {
      std::cerr << "Failed to open file for writing: " << filename << std::endl;
      return false;
    }
    file << j.dump(2);  // Pretty print with 2-space indent
    file.close();
    return true;
  } catch (const std::exception& e) {
    std::cerr << "Save error: " << e.what() << std::endl;
    return false;
  }
}

bool WhiteboardDocument::load_from_file(const std::string& filename) {
  try {
    std::ifstream file(filename);
    if (!file.is_open()) {
      std::cerr << "Failed to open file for reading: " << filename << std::endl;
      return false;
    }
    nlohmann::json j;
    file >> j;
    file.close();
    
    from_json(j);
    return true;
  } catch (const std::exception& e) {
    std::cerr << "Load error: " << e.what() << std::endl;
    return false;
  }
}

} // namespace whiteboard

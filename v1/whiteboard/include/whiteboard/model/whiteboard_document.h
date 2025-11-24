/**
 * \file whiteboard_document.h
 * \brief Central model for the whiteboard application - single source of truth.
 */

#pragma once

#include "whiteboard/model/document_observer.h"
#include "whiteboard/model/document_state.h"
#include "whiteboard/types.h"
#include <nanogui/common.h>
#include <nanogui/vector.h>
#include <nlohmann/json.hpp>
#include <stack>
#include <string>
#include <vector>

namespace whiteboard {

/**
 * \class WhiteboardDocument
 * \brief The Model in MVC - owns all application data and state.
 *
 * This class is the single source of truth for the entire whiteboard application.
 * All UI components (Views) observe this model and update themselves when notified
 * of changes. All user actions (via Controllers) modify this model, which then
 * notifies observers.
 *
 * The model owns:
 * - Stroke data (drawings, shapes, text, images)
 * - Selection state (which strokes are selected)
 * - Tool state (current tool and its properties)
 * - View state (zoom, pan, grid/guide visibility)
 * - History (undo/redo stacks)
 * - Guides (alignment helpers)
 */
class WhiteboardDocument {
public:
  WhiteboardDocument();
  ~WhiteboardDocument() = default;

  // === Stroke Management ===

  /**
   * \brief Get all strokes (read-only).
   */
  const std::vector<Stroke> &get_strokes() const { return m_strokes; }

  /**
   * \brief Add a new stroke to the document.
   * \param stroke The stroke to add
   *
   * Pushes current state to undo stack before adding.
   * Notifies observers via on_strokes_changed().
   */
  void add_stroke(const Stroke &stroke);

  /**
   * \brief Remove strokes by indices.
   * \param indices Vector of stroke indices to remove
   *
   * Validates indices, pushes to undo stack, removes strokes.
   * Notifies observers via on_strokes_changed().
   */
  void remove_strokes(const std::vector<int> &indices);

  /**
   * \brief Update an existing stroke.
   * \param index Index of stroke to update
   * \param stroke New stroke data
   *
   * Pushes to undo stack before updating.
   * Notifies observers via on_strokes_changed().
   */
  void update_stroke(int index, const Stroke &stroke);

  /**
   * \brief Find stroke at a given point.
   * \param x X coordinate in canvas space
   * \param y Y coordinate in canvas space
   * \param margin Hit test margin in pixels
   * \return Index of stroke, or -1 if none found
   */
  int find_stroke_at_point(float x, float y, float margin = 10.0f) const;

  /**
   * \brief Find all strokes within a rectangle.
   * \param x1 Left edge
   * \param y1 Top edge
   * \param x2 Right edge
   * \param y2 Bottom edge
   * \return Vector of stroke indices
   */
  std::vector<int> find_strokes_in_rect(float x1, float y1, float x2, float y2) const;

  // === Selection Management ===

  /**
   * \brief Get selected stroke indices (read-only).
   */
  const std::vector<int> &get_selected_indices() const { return m_selected_indices; }

  /**
   * \brief Set the selection to specific indices.
   * \param indices Vector of stroke indices to select
   *
   * Notifies observers via on_selection_changed().
   */
  void set_selection(const std::vector<int> &indices);

  /**
   * \brief Clear the selection.
   *
   * Notifies observers via on_selection_changed().
   */
  void clear_selection();

  /**
   * \brief Check if a stroke is selected.
   * \param index Stroke index
   * \return True if selected
   */
  bool is_selected(int index) const;

  // === Tool State ===

  /**
   * \brief Get the current tool.
   */
  Tool get_current_tool() const { return m_current_tool; }

  /**
   * \brief Set the current tool.
   * \param tool New tool
   *
   * Notifies observers via on_tool_changed().
   */
  void set_current_tool(Tool tool);

  /**
   * \brief Get pending SVG shape ID (for drag-and-drop placement).
   */
  std::string get_pending_svg_shape() const { return m_pending_svg_shape; }

  /**
   * \brief Set pending SVG shape ID for next placement.
   * \param shape_id Shape ID from library
   */
  void set_pending_svg_shape(const std::string& shape_id) { m_pending_svg_shape = shape_id; }

  /**
   * \brief Clear pending SVG shape.
   */
  void clear_pending_svg_shape() { m_pending_svg_shape.clear(); }

  // === Export ===

  /**
   * \brief Export canvas to PNG file.
   * \param file_path Output file path
   * \param width Image width (0 = use canvas size)
   * \param height Image height (0 = use canvas size)
   * \return True if successful
   */
  bool export_to_png(const std::string& file_path, int width = 0, int height = 0);

  /**
   * \brief Export canvas to SVG file.
   * \param file_path Output file path
   * \return True if successful
   */
  bool export_to_svg(const std::string& file_path);

  // === Tool Properties ===

  /**
   * \brief Get current stroke color.
   */
  nanogui::Color get_stroke_color() const { return m_stroke_color; }

  /**
   * \brief Set stroke color.
   * \param color New color
   *
   * Notifies observers via on_properties_changed().
   */
  void set_stroke_color(const nanogui::Color &color);

  /**
   * \brief Get current stroke width.
   */
  float get_stroke_width() const { return m_stroke_width; }

  /**
   * \brief Set stroke width.
   * \param width New width
   *
   * Notifies observers via on_properties_changed().
   */
  void set_stroke_width(float width);

  /**
   * \brief Get current fill style.
   */
  FillStyle get_fill_style() const { return m_fill_style; }

  /**
   * \brief Set fill style.
   * \param style New fill style
   *
   * Notifies observers via on_properties_changed().
   */
  void set_fill_style(FillStyle style);

  /**
   * \brief Get current fill color.
   */
  nanogui::Color get_fill_color() const { return m_fill_color; }

  /**
   * \brief Set fill color.
   * \param color New fill color
   *
   * Notifies observers via on_properties_changed().
   */
  void set_fill_color(const nanogui::Color &color);

  // === Text Properties ===

  /**
   * \brief Get current font face.
   */
  std::string get_font_face() const { return m_font_face; }

  /**
   * \brief Set font face.
   * \param face New font face name
   *
   * Notifies observers via on_properties_changed().
   */
  void set_font_face(const std::string &face);

  /**
   * \brief Get current font size.
   */
  float get_font_size() const { return m_font_size; }

  /**
   * \brief Set font size.
   * \param size New font size
   *
   * Notifies observers via on_properties_changed().
   */
  void set_font_size(float size);

  /**
   * \brief Get current text alignment.
   */
  int get_text_align() const { return m_text_align; }

  /**
   * \brief Set text alignment.
   * \param align New alignment (NVG_ALIGN_* flags)
   *
   * Notifies observers via on_properties_changed().
   */
  void set_text_align(int align);

  // === View State ===

  /**
   * \brief Get current zoom level.
   */
  float get_zoom() const { return m_zoom; }

  /**
   * \brief Set zoom level.
   * \param zoom New zoom (clamped to 0.25-6.0)
   *
   * Notifies observers via on_view_changed().
   */
  void set_zoom(float zoom);

  /**
   * \brief Get current pan offset.
   */
  nanogui::Vector2f get_pan_offset() const { return m_pan_offset; }

  /**
   * \brief Set pan offset.
   * \param offset New pan offset
   *
   * Notifies observers via on_view_changed().
   */
  void set_pan_offset(const nanogui::Vector2f &offset);

  /**
   * \brief Check if grid is visible.
   */
  bool get_grid_visible() const { return m_grid_visible; }

  /**
   * \brief Set grid visibility.
   * \param visible New visibility state
   *
   * Notifies observers via on_view_changed().
   */
  void set_grid_visible(bool visible) {
    m_grid_visible = visible;
    notify_view_changed();
  }

  /**
   * \brief Check if guides are visible.
   */
  bool get_guides_visible() const { return m_guides_visible; }

  /**
   * \brief Set guide visibility.
   * \param visible New visibility state
   *
   * Notifies observers via on_view_changed().
   */
  void set_guides_visible(bool visible) {
    m_guides_visible = visible;
    notify_view_changed();
  }

  /**
   * \brief Check if dark mode is enabled.
   */
  bool get_dark_mode() const { return m_dark_mode; }

  /**
   * \brief Set dark mode.
   * \param enabled New dark mode state
   *
   * Notifies observers via on_view_changed().
   */
  void set_dark_mode(bool enabled);

  /**
   * \brief Get canvas background color based on current theme.
   */
  nanogui::Color get_canvas_background() const {
    return m_dark_mode ? m_canvas_bg_dark : m_canvas_bg_light;
  }

  /**
   * \brief Check if snap to grid is enabled.
   */
  bool get_snap_enabled() const { return m_snap_enabled; }

  /**
   * \brief Set snap to grid enabled.
   * \param enabled New enabled state
   *
   * Notifies observers via on_view_changed().
   */
  void set_snap_enabled(bool enabled);

  /**
   * \brief Get snap distance in pixels.
   */
  float get_snap_distance() const { return m_snap_distance; }

  /**
   * \brief Set snap distance in pixels.
   * \param distance New snap distance
   */
  void set_snap_distance(float distance) { m_snap_distance = distance; }

  // === Guide Management ===

  /**
   * \brief Get all guides (read-only).
   */
  const std::vector<Guide> &get_guides() const { return m_guides; }

  /**
   * \brief Add a new guide.
   * \param guide Guide to add
   *
   * Notifies observers via on_strokes_changed().
   */
  void add_guide(const Guide &guide);

  /**
   * \brief Remove a guide by index.
   * \param index Guide index
   *
   * Notifies observers via on_strokes_changed().
   */
  void remove_guide(int index);

  /**
   * \brief Update a guide's position.
   * \param index Guide index
   * \param position New position
   *
   * Notifies observers via on_strokes_changed().
   */
  void update_guide(int index, float position);

  // === History (Undo/Redo) ===

  /**
   * \brief Undo the last action.
   *
   * Pops from undo stack, pushes current state to redo stack.
   * Notifies observers via on_strokes_changed() and on_selection_changed().
   */
  void undo();

  /**
   * \brief Redo the last undone action.
   *
   * Pops from redo stack, pushes current state to undo stack.
   * Notifies observers via on_strokes_changed() and on_selection_changed().
   */
  void redo();

  /**
   * \brief Check if undo is available.
   */
  bool can_undo() const { return !m_undo_stack.empty(); }

  /**
   * \brief Check if redo is available.
   */
  bool can_redo() const { return !m_redo_stack.empty(); }

  // === Layer Management ===

  /**
   * \brief Set stroke visibility.
   * \param index Stroke index
   * \param visible New visibility state
   *
   * Notifies observers via on_strokes_changed().
   */
  void set_stroke_visible(int index, bool visible);

  /**
   * \brief Set stroke locked state.
   * \param index Stroke index
   * \param locked New locked state
   *
   * Notifies observers via on_strokes_changed().
   */
  void set_stroke_locked(int index, bool locked);

  /**
   * \brief Set stroke name.
   * \param index Stroke index
   * \param name New name
   *
   * Notifies observers via on_strokes_changed().
   */
  void set_stroke_name(int index, const std::string &name);

  /**
   * \brief Reorder a stroke (change z-order).
   * \param from_index Current index
   * \param to_index Target index
   *
   * Pushes to undo stack before reordering.
   * Notifies observers via on_strokes_changed().
   */
  void reorder_stroke(int from_index, int to_index);

  // === Serialization ===

  /**
   * \brief Serialize document to JSON.
   * \return JSON representation of entire document state
   *
   * Serializes all strokes, canvas state, tool state, guides, etc.
   */
  nlohmann::json to_json() const;

  /**
   * \brief Deserialize document from JSON.
   * \param j JSON object to deserialize
   *
   * Clears current state and loads from JSON.
   * Notifies observers after loading.
   */
  void from_json(const nlohmann::json &j);

  /**
   * \brief Save document to file.
   * \param filename Path to save file (.whiteboard extension)
   * \return True if successful, false on error
   */
  bool save_to_file(const std::string &filename);

  /**
   * \brief Load document from file.
   * \param filename Path to load file
   * \return True if successful, false on error
   */
  bool load_from_file(const std::string &filename);

  // === Observer Pattern ===

  /**
   * \brief Register an observer.
   * \param observer Observer to add
   */
  void add_observer(IDocumentObserver *observer);

  /**
   * \brief Unregister an observer.
   * \param observer Observer to remove
   */
  void remove_observer(IDocumentObserver *observer);

private:
  // === Data ===
  std::vector<Stroke> m_strokes;
  std::vector<int> m_selected_indices;
  std::vector<Guide> m_guides;

  // === Current Tool State ===
  Tool m_current_tool;
  nanogui::Color m_stroke_color;
  float m_stroke_width;
  FillStyle m_fill_style;
  nanogui::Color m_fill_color;
  std::string m_font_face;
  float m_font_size;
  int m_text_align;

  // === View State ===
  float m_zoom;
  nanogui::Vector2f m_pan_offset;
  bool m_grid_visible;
  bool m_guides_visible;
  bool m_snap_enabled;
  float m_snap_distance;
  bool m_dark_mode;
  nanogui::Color m_canvas_bg_light;
  nanogui::Color m_canvas_bg_dark;
  
  // === SVG Shape Placement ===
  std::string m_pending_svg_shape;

  // === History ===
  std::stack<DocumentState> m_undo_stack;
  std::stack<DocumentState> m_redo_stack;

  // === Observers ===
  std::vector<IDocumentObserver *> m_observers;

  // === Notification Helpers ===
  void notify_strokes_changed();
  void notify_selection_changed();
  void notify_tool_changed();
  void notify_properties_changed();
  void notify_view_changed();

  // === Helper Methods ===
  void push_undo_state();
  DocumentState capture_state() const;
  void restore_state(const DocumentState &state);
};

} // namespace whiteboard

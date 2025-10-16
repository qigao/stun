/**
 * \file canvas_controller.h
 * \brief Controller component of Canvas MVC - handles input and updates model.
 */

#pragma once

#include "whiteboard/model/whiteboard_document.h"
#include "whiteboard/types.h"
#include <chrono>
#include <nanogui/vector.h>

namespace whiteboard {

// Forward declarations
class CanvasView;
class SVGShapeLibrary;

/**
 * \class CanvasController
 * \brief The Controller in Canvas MVC - processes input and updates the model.
 *
 * CanvasController is responsible for:
 * - Processing mouse and keyboard input
 * - Implementing tool-specific behavior (pen, select, shapes, etc.)
 * - Updating the model when actions complete
 * - Managing interaction state (drawing, selecting, moving, etc.)
 * - Applying snapping logic
 *
 * CanvasController does NOT:
 * - Render anything
 * - Store persistent data (that's the model's job)
 * - Know about other UI components
 */
class CanvasController {
public:
  /**
   * \brief Constructor.
   * \param document The shared document model
   * \param view The canvas view
   */
  CanvasController(WhiteboardDocument *document, CanvasView *view);

  // === Input Handling ===

  /**
   * \brief Handle mouse button down event.
   * \param canvas_pos Position in canvas coordinates
   * \param modifiers Keyboard modifiers
   * \return True if handled
   */
  bool handle_mouse_down(const nanogui::Vector2f &canvas_pos, int modifiers);

  /**
   * \brief Handle mouse drag event.
   * \param canvas_pos Position in canvas coordinates
   * \param modifiers Keyboard modifiers
   * \return True if handled
   */
  bool handle_mouse_drag(const nanogui::Vector2f &canvas_pos, int modifiers);

  /**
   * \brief Handle mouse button up event.
   * \param canvas_pos Position in canvas coordinates
   * \param modifiers Keyboard modifiers
   * \return True if handled
   */
  bool handle_mouse_up(const nanogui::Vector2f &canvas_pos, int modifiers);

  /**
   * \brief Handle scroll event (zoom).
   * \param canvas_pos Position in canvas coordinates
   * \param delta Scroll delta
   * \return True if handled
   */
  bool handle_scroll(const nanogui::Vector2f &canvas_pos, float delta);

  /**
   * \brief Handle key press event.
   * \param key Key code
   * \param modifiers Keyboard modifiers
   * \return True if handled
   */
  bool handle_key_press(int key, int modifiers);

  /**
   * \brief Handle key release event.
   * \param key Key code
   * \param modifiers Keyboard modifiers
   * \return True if handled
   */
  bool handle_key_release(int key, int modifiers);

  /**
   * \brief Set the shape library for SVG shape operations.
   * \param library Shape library instance
   */
  void set_shape_library(SVGShapeLibrary *library) { m_shape_library = library; }

private:
  // === Model and View ===
  WhiteboardDocument *m_document;
  CanvasView *m_view;
  SVGShapeLibrary *m_shape_library;

  // === Interaction State ===
  enum class Mode {
    None,
    Drawing,
    AreaSelecting,
    Moving,
    Resizing,
    Rotating,
    Panning,
    CreatingGuide
  };
  Mode m_mode;

  // === Interaction Data ===
  nanogui::Vector2f m_interaction_start;
  Stroke m_temp_stroke;
  std::vector<Point> m_original_positions;
  float m_rotation_start_angle;
  nanogui::Vector2f m_rotation_center;

  // === Double-click Detection ===
  std::chrono::steady_clock::time_point m_last_click_time;
  nanogui::Vector2f m_last_click_pos;
  int m_last_clicked_stroke;

  // === Tool Handlers ===

  /**
   * \brief Handle pen tool mouse down.
   */
  void handle_pen_tool_down(const nanogui::Vector2f &pos);

  /**
   * \brief Handle pen tool mouse drag.
   */
  void handle_pen_tool_drag(const nanogui::Vector2f &pos);

  /**
   * \brief Handle pen tool mouse up.
   */
  void handle_pen_tool_up(const nanogui::Vector2f &pos);

  /**
   * \brief Handle shape tool mouse down.
   */
  void handle_shape_tool_down(const nanogui::Vector2f &pos);

  /**
   * \brief Handle shape tool mouse drag.
   */
  void handle_shape_tool_drag(const nanogui::Vector2f &pos);

  /**
   * \brief Handle shape tool mouse up.
   */
  void handle_shape_tool_up(const nanogui::Vector2f &pos);

  /**
   * \brief Handle select tool mouse down.
   */
  void handle_select_tool_down(const nanogui::Vector2f &pos, int modifiers);

  /**
   * \brief Handle select tool mouse drag.
   */
  void handle_select_tool_drag(const nanogui::Vector2f &pos);

  /**
   * \brief Handle select tool mouse up.
   */
  void handle_select_tool_up(const nanogui::Vector2f &pos);

  /**
   * \brief Handle pan tool mouse down.
   */
  void handle_pan_tool_down(const nanogui::Vector2f &pos);

  /**
   * \brief Handle pan tool mouse drag.
   */
  void handle_pan_tool_drag(const nanogui::Vector2f &pos);

  /**
   * \brief Handle pan tool mouse up.
   */
  void handle_pan_tool_up(const nanogui::Vector2f &pos);

  /**
   * \brief Handle SVG shape tool mouse down.
   */
  void handle_svg_shape_tool_down(const nanogui::Vector2f &pos);

  /**
   * \brief Handle SVG shape tool mouse drag.
   */
  void handle_svg_shape_tool_drag(const nanogui::Vector2f &pos);

  /**
   * \brief Handle SVG shape tool mouse up.
   */
  void handle_svg_shape_tool_up(const nanogui::Vector2f &pos);

  // === Helper Methods ===

  /**
   * \brief Snap a point to grid if enabled.
   * \param p Point to snap
   * \return Snapped point
   */
  Point snap_point(const Point &p);

  /**
   * \brief Check if clicking on a selection handle.
   * \param pos Position in canvas coordinates
   * \return True if clicking on handle
   */
  bool is_clicking_selection_handle(const nanogui::Vector2f &pos);

  /**
   * \brief Check if clicking on rotation handle.
   * \param pos Position in canvas coordinates
   * \return True if clicking on handle
   */
  bool is_clicking_rotation_handle(const nanogui::Vector2f &pos);

  /**
   * \brief Check if this is a double-click.
   * \param pos Current click position
   * \param stroke_index Clicked stroke index
   * \return True if double-click detected
   */
  bool is_double_click(const nanogui::Vector2f &pos, int stroke_index);

  /**
   * \brief Handle double-click on SVG shape.
   * \param stroke_index Index of clicked stroke
   * \param pos Click position in canvas coordinates
   */
  void handle_svg_double_click(int stroke_index, const nanogui::Vector2f &pos);
};

} // namespace whiteboard

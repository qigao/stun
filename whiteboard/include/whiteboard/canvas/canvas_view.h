/**
 * \file canvas_view.h
 * \brief View component of Canvas MVC - handles rendering only.
 */

#pragma once

#include "whiteboard/model/document_observer.h"
#include "whiteboard/model/whiteboard_document.h"
#include "whiteboard/types.h"
#include <nanogui/canvas.h>
#include <nanogui/vector.h>
#include <memory>

namespace whiteboard {

// Forward declarations
class CanvasController;
class SVGRenderer;

namespace ddf {
class DDFDocument;
struct Shape;
}

/**
 * \class CanvasView
 * \brief The View in Canvas MVC - pure rendering and coordinate transforms.
 *
 * CanvasView is responsible for:
 * - Rendering all strokes from the model
 * - Rendering grid, guides, rulers, minimap
 * - Rendering selection boxes and handles
 * - Rendering temporary stroke during drawing
 * - Coordinate transformations (local ↔ canvas)
 * - Delegating input events to the controller
 *
 * CanvasView does NOT:
 * - Modify the model directly
 * - Implement tool logic
 * - Handle business logic
 */
class CanvasView : public nanogui::Canvas, public IDocumentObserver {
public:
  /**
   * \brief Constructor.
   * \param parent Parent widget
   * \param document The shared document model
   */
  CanvasView(nanogui::Widget *parent, WhiteboardDocument *document);

  /**
   * \brief Destructor - unregisters from document.
   */
  ~CanvasView() override;

  // === Rendering ===

  /**
   * \brief Main draw method - renders everything.
   * \param ctx NanoVG context
   */
  void draw(NVGcontext *ctx) override;

  // === Coordinate Transforms ===

  /**
   * \brief Convert local widget coordinates to canvas coordinates.
   * \param local Local coordinates (relative to widget)
   * \return Canvas coordinates (accounting for zoom, pan, rulers)
   */
  nanogui::Vector2f local_to_canvas(const nanogui::Vector2i &local) const;

  /**
   * \brief Convert canvas coordinates to local widget coordinates.
   * \param canvas Canvas coordinates
   * \return Local coordinates (relative to widget)
   */
  nanogui::Vector2f canvas_to_local(const nanogui::Vector2f &canvas) const;

  /**
   * \brief Convert canvas coordinates to global screen coordinates.
   * \param canvas Canvas coordinates
   * \return Global screen coordinates
   */
  nanogui::Vector2f canvas_to_global(const nanogui::Vector2f &canvas) const;

  // === Temporary State (not in model) ===

  /**
   * \brief Set the current stroke being drawn.
   * \param stroke Temporary stroke
   */
  void set_current_stroke(const Stroke &stroke);

  /**
   * \brief Clear the current stroke.
   */
  void clear_current_stroke();

  /**
   * \brief Set the selection marquee rectangle.
   * \param start Start point in canvas coordinates
   * \param end End point in canvas coordinates
   */
  void set_selection_marquee(const nanogui::Vector2f &start, const nanogui::Vector2f &end);

  /**
   * \brief Clear the selection marquee.
   */
  void clear_selection_marquee();

  /**
   * \brief Set the snap indicator position.
   * \param point Snap point in canvas coordinates
   */
  void set_snap_indicator(const nanogui::Vector2f &point);

  /**
   * \brief Clear the snap indicator.
   */
  void clear_snap_indicator();

  /**
   * \brief Check if mouse position is clicking the rotation handle.
   * \param p Mouse position in global coordinates
   * \return True if clicking rotation handle, false otherwise
   */
  bool is_clicking_rotation_handle(const nanogui::Vector2i &p) const;

  /**
   * \brief Check if position is in the top ruler area.
   * \param p Position in local widget coordinates
   * \return True if in top ruler
   */
  bool is_in_top_ruler(const nanogui::Vector2i &p) const;

  /**
   * \brief Check if position is in the left ruler area.
   * \param p Position in local widget coordinates
   * \return True if in left ruler
   */
  bool is_in_left_ruler(const nanogui::Vector2i &p) const;

  /**
   * \brief Set temporary guide for preview during creation.
   * \param type Guide type
   * \param position Guide position in canvas coordinates
   */
  void set_temp_guide(Guide::Type type, float position);

  /**
   * \brief Clear temporary guide.
   */
  void clear_temp_guide();

  // === IDocumentObserver Implementation ===

  /**
   * \brief Called when strokes change - triggers redraw.
   */
  void on_strokes_changed() override;

  /**
   * \brief Called when selection changes - triggers redraw.
   */
  void on_selection_changed() override;

  /**
   * \brief Called when view state changes - updates viewport.
   */
  void on_view_changed() override;

  // === Input Event Delegation ===

  /**
   * \brief Mouse button event - delegates to controller.
   */
  bool mouse_button_event(const nanogui::Vector2i &p, int button, bool down,
                          int modifiers) override;

  /**
   * \brief Mouse drag event - delegates to controller.
   */
  bool mouse_drag_event(const nanogui::Vector2i &p, const nanogui::Vector2i &rel, int button,
                        int modifiers) override;

  /**
   * \brief Mouse motion event - tracks cursor for pending shape preview.
   */
  bool mouse_motion_event(const nanogui::Vector2i &p, const nanogui::Vector2i &rel, int button,
                          int modifiers) override;

  /**
   * \brief Scroll event - delegates to controller.
   */
  bool scroll_event(const nanogui::Vector2i &p, const nanogui::Vector2f &rel) override;

  /**
   * \brief Keyboard event - delegates to controller.
   */
  bool keyboard_event(int key, int scancode, int action, int modifiers) override;

  // === Controller Access ===

  /**
   * \brief Set the controller for this view.
   * \param controller Controller instance
   */
  void set_controller(CanvasController *controller) { m_controller = controller; }

  /**
   * \brief Set callback for right-click events.
   * \param callback Function to call on right-click with global position
   */
  void set_right_click_callback(std::function<void(const nanogui::Vector2i&)> callback) {
    m_right_click_callback = callback;
  }

  /**
   * \brief Import an image file at a specific position.
   * \param file_path Path to image file
   * \param pos Position in canvas coordinates
   * \return True if successful
   */
  bool import_image(const std::string &file_path, const nanogui::Vector2f &pos);

  /**
   * \brief Import an SVG shape file at a specific position.
   * \param file_path Path to SVG file
   * \param pos Position in canvas coordinates
   * \return True if successful
   */
  bool import_svg_shape(const std::string &file_path, const nanogui::Vector2f &pos);

  /**
   * \brief Check if a file is an image file.
   * \param file_path Path to file
   * \return True if image file
   */
  static bool is_image_file(const std::string &file_path);

  /**
   * \brief Check if a file is an SVG file.
   * \param file_path Path to file
   * \return True if SVG file
   */
  static bool is_svg_file(const std::string &file_path);

  /**
   * \brief Get which text parameter is at a given canvas position for an SVG stroke.
   * \param stroke_index Index of the stroke
   * \param canvas_pos Position in canvas coordinates
   * \return Parameter name, or empty string if none found
   */
  std::string get_text_parameter_at_position(int stroke_index, const nanogui::Vector2f &canvas_pos);

  /**
   * \brief Build or update the text bounds cache for a stroke.
   * \param stroke_index Index of the stroke
   * \return True if cache was built successfully
   */
  bool update_text_bounds_cache(int stroke_index) const;

  // === DDF Integration ===

  /**
   * \brief Set the DDF document for rendering.
   * \param document Shared pointer to DDF document
   */
  void set_ddf_document(std::shared_ptr<ddf::DDFDocument> document);

  /**
   * \brief Get the current DDF document.
   * \return Shared pointer to DDF document, or nullptr if none set
   */
  std::shared_ptr<ddf::DDFDocument> get_ddf_document() const { return m_ddf_document; }

  /**
   * \brief Handle mouse events for DDF elements.
   * \param canvas_pos Position in canvas coordinates
   * \param event_type Type of mouse event (click, hover, etc.)
   * \return True if a DDF element handled the event
   */
  bool handle_ddf_mouse_event(const nanogui::Vector2f &canvas_pos, const std::string &event_type);

  /**
   * \brief Update DDF element pseudo-states based on mouse position.
   * \param canvas_pos Position in canvas coordinates
   */
  void update_ddf_pseudo_states(const nanogui::Vector2f &canvas_pos);
  
  /**
   * \brief Set SVG shape library for DDF rendering
   * \param library The SVG shape library
   */
  void set_svg_shape_library(class SVGShapeLibrary *library);
  
  /**
   * \brief Convert a DDF shape to an editable stroke
   * \param shape The DDF shape to convert
   * \return A stroke representation of the shape
   */
  Stroke convert_ddf_shape_to_stroke(const ddf::Shape *shape);
  
  /**
   * \brief Convert hex color string to nanogui Color
   * \param hex Hex color string (e.g., "#3498db")
   * \return nanogui Color object
   */
  nanogui::Color hex_to_color(const std::string &hex);

private:
  // === Model and Controller ===
  WhiteboardDocument *m_document;
  CanvasController *m_controller;

  // === SVG Rendering ===
  SVGRenderer *m_svg_renderer;

  // === DDF Integration ===
  std::shared_ptr<ddf::DDFDocument> m_ddf_document;
  class SVGShapeLibrary *m_svg_shape_library = nullptr;

  // === Temporary State (not persisted in model) ===
  Stroke m_current_stroke;
  bool m_has_current_stroke;

  nanogui::Vector2f m_marquee_start;
  nanogui::Vector2f m_marquee_end;
  bool m_has_marquee;

  nanogui::Vector2f m_snap_point;
  bool m_has_snap_point;

  Guide::Type m_temp_guide_type;
  float m_temp_guide_position;
  bool m_has_temp_guide;

  // Cursor position tracking for pending shape preview
  nanogui::Vector2f m_last_cursor_pos;
  bool m_has_cursor_pos;

  // Right-click callback
  std::function<void(const nanogui::Vector2i&)> m_right_click_callback;

  // NanoVG context (cached from draw calls)
  NVGcontext *m_nvg_context;

  // === Text Bounds Cache ===
  struct TextBoundsInfo {
    std::string parameter_name;
    float x, y, width, height;
    std::string text_anchor; // "start", "middle", or "end"
  };
  
  // Cache: stroke_index -> list of text bounds
  mutable std::map<int, std::vector<TextBoundsInfo>> m_text_bounds_cache;
  
  // Cache invalidation tracking
  mutable std::map<int, std::string> m_cached_svg_data; // stroke_index -> svg_data hash

  // === Rendering Constants ===
  static constexpr float RULER_SIZE = 30.0f;
  static constexpr float GRID_SIZE = 20.0f;
  static constexpr float MINIMAP_SIZE = 150.0f;

  // === Rendering Helpers ===

  /**
   * \brief Draw the infinite grid.
   */
  void draw_grid(NVGcontext *ctx);

  /**
   * \brief Draw guide lines.
   */
  void draw_guides(NVGcontext *ctx);

  /**
   * \brief Draw all strokes from the model.
   */
  void draw_strokes(NVGcontext *ctx);

  /**
   * \brief Draw a single stroke.
   */
  void draw_stroke(NVGcontext *ctx, const Stroke &stroke, int stroke_index);

  /**
   * \brief Draw an SVG stroke.
   */
  void draw_svg_stroke(NVGcontext *ctx, const Stroke &stroke, int stroke_index);

  /**
   * \brief Draw selection box around selected strokes.
   */
  void draw_selection(NVGcontext *ctx);

  /**
   * \brief Draw snap indicator at snap point.
   */
  void draw_snap_indicator(NVGcontext *ctx);

  /**
   * \brief Draw the current stroke being drawn.
   */
  void draw_current_stroke(NVGcontext *ctx);

  /**
   * \brief Draw the selection marquee rectangle.
   */
  void draw_marquee(NVGcontext *ctx);

  /**
   * \brief Draw pending shape preview following cursor.
   */
  void draw_pending_shape_preview(NVGcontext *ctx);

  /**
   * \brief Draw rulers at top and left edges.
   */
  void draw_rulers(NVGcontext *ctx);

  /**
   * \brief Draw minimap in corner.
   */
  void draw_minimap(NVGcontext *ctx);

  /**
   * \brief Draw coordinate display.
   */
  void draw_coordinate_display(NVGcontext *ctx);

  /**
   * \brief Draw snap feedback indicator.
   */
  void draw_snap_feedback(NVGcontext *ctx);

  /**
   * \brief Draw DDF elements from the DDF document.
   */
  void draw_ddf_elements(NVGcontext *ctx);

  // === Helper Methods ===

  /**
   * \brief Convert Point to Vector2f.
   */
  nanogui::Vector2f to_vec(const Point &p) const { return nanogui::Vector2f(p.x, p.y); }

  /**
   * \brief Convert Vector2i to Vector2f.
   */
  nanogui::Vector2f to_vec(const nanogui::Vector2i &v) const {
    return nanogui::Vector2f(static_cast<float>(v.x()), static_cast<float>(v.y()));
  }
};

} // namespace whiteboard

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

namespace whiteboard {

// Forward declaration
class CanvasController;

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

private:
  // === Model and Controller ===
  WhiteboardDocument *m_document;
  CanvasController *m_controller;

  // === Temporary State (not persisted in model) ===
  Stroke m_current_stroke;
  bool m_has_current_stroke;

  nanogui::Vector2f m_marquee_start;
  nanogui::Vector2f m_marquee_end;
  bool m_has_marquee;

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
  void draw_stroke(NVGcontext *ctx, const Stroke &stroke);

  /**
   * \brief Draw selection box around selected strokes.
   */
  void draw_selection(NVGcontext *ctx);

  /**
   * \brief Draw the current stroke being drawn.
   */
  void draw_current_stroke(NVGcontext *ctx);

  /**
   * \brief Draw the selection marquee rectangle.
   */
  void draw_marquee(NVGcontext *ctx);

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

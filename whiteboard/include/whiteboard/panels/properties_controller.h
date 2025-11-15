/**
 * \file properties_controller.h
 * \brief Controller for the properties panel - mediates between view and model.
 */

#pragma once

#include "whiteboard/types.h"
#include <functional>
#include <nanogui/common.h>

// Forward declaration (PropertiesPanelModule is in global namespace)
class PropertiesPanelModule;

namespace whiteboard {

// Forward declarations
class WhiteboardDocument;

/**
 * \class PropertiesController
 * \brief Controller that mediates between PropertiesPanelModule (View) and WhiteboardDocument
 * (Model).
 *
 * This controller handles property changes from the properties panel UI and applies them
 * to selected shapes in the document. It follows the MVC pattern by:
 * - Receiving user actions from the View (PropertiesPanelModule)
 * - Modifying the Model (WhiteboardDocument) via update_stroke()
 * - Letting the Observer pattern handle View updates
 *
 * Key responsibilities:
 * - Apply property changes to selected shapes only
 * - Skip locked shapes when applying changes
 * - Handle default property updates when no shapes are selected
 * - Ensure thread safety (all operations on main UI thread)
 */
class PropertiesController {
public:
  /**
   * \brief Constructor.
   * \param document Pointer to the document model (must not be null)
   * \param view Pointer to the properties panel view (must not be null)
   */
  PropertiesController(WhiteboardDocument *document, ::PropertiesPanelModule *view);

  /**
   * \brief Destructor.
   */
  ~PropertiesController();

  // === Stroke Properties ===

  /**
   * \brief Set stroke color for selected shapes.
   * \param color New stroke color
   *
   * If shapes are selected: Updates color for all selected unlocked shapes.
   * If no selection: Updates default stroke color for new shapes.
   */
  void set_stroke_color(const nanogui::Color &color);

  /**
   * \brief Set fill color for selected shapes.
   * \param color New fill color
   *
   * If shapes are selected: Updates fill color for all selected unlocked shapes.
   * If no selection: Updates default fill color for new shapes.
   * Note: Skips shapes that don't support fill (Line, Arrow).
   */
  void set_fill_color(const nanogui::Color &color);

  /**
   * \brief Set stroke width for selected shapes.
   * \param width New stroke width (will be clamped to minimum 0.5)
   *
   * If shapes are selected: Updates width for all selected unlocked shapes.
   * If no selection: Updates default stroke width for new shapes.
   */
  void set_stroke_width(float width);

  /**
   * \brief Set stroke style for selected shapes.
   * \param style New stroke style (Solid, Dashed, Dotted)
   *
   * If shapes are selected: Updates stroke style for all selected unlocked shapes.
   * If no selection: Updates default stroke style for new shapes.
   */
  void set_stroke_style(StrokeStyle style);

  /**
   * \brief Set opacity for selected shapes.
   * \param opacity New opacity (will be clamped to 0.0-1.0)
   *
   * Applies to selected unlocked shapes only.
   */
  void set_opacity(float opacity);

  /**
   * \brief Set rotation for selected shapes.
   * \param rotation New rotation in radians (will be normalized to 0-2π)
   *
   * Applies to selected unlocked shapes only.
   */
  void set_rotation(float rotation);

  /**
   * \brief Set corner radius for selected shapes.
   * \param radius New corner radius (will be clamped to non-negative)
   *
   * Applies only to Rectangle and Diamond shapes.
   * Skips other shape types.
   */
  void set_corner_radius(float radius);

  /**
   * \brief Set SVG scale X for selected SVG shapes.
   * \param scale_x Horizontal scale factor (0.1-3.0)
   *
   * Applies only to SVG shapes.
   */
  void set_svg_scale_x(float scale_x);

  /**
   * \brief Set SVG scale Y for selected SVG shapes.
   * \param scale_y Vertical scale factor (0.1-3.0)
   *
   * Applies only to SVG shapes.
   */
  void set_svg_scale_y(float scale_y);

  // === Text Properties ===

  /**
   * \brief Set font face for selected text shapes.
   * \param face New font face name (e.g., "sans", "sans-bold", "serif", "mono")
   *
   * Applies only to Text shapes. Skips non-text shapes.
   */
  void set_font_face(const std::string &face);

  /**
   * \brief Set font size for selected text shapes.
   * \param size New font size (will be clamped to 8-72)
   *
   * Applies only to Text shapes. Skips non-text shapes.
   */
  void set_font_size(float size);

  /**
   * \brief Set text alignment for selected text shapes.
   * \param align New alignment (NVG_ALIGN_* flags)
   *
   * Applies only to Text shapes. Skips non-text shapes.
   */
  void set_text_align(int align);

  // === Layer Operations ===

  /**
   * \brief Bring selected shapes forward one layer.
   *
   * Moves each selected shape up one position in the z-order.
   */
  void bring_forward();

  /**
   * \brief Send selected shapes backward one layer.
   *
   * Moves each selected shape down one position in the z-order.
   */
  void send_backward();

  /**
   * \brief Bring selected shapes to front.
   *
   * Moves each selected shape to the top of the z-order.
   */
  void bring_to_front();

  /**
   * \brief Send selected shapes to back.
   *
   * Moves each selected shape to the bottom of the z-order.
   */
  void send_to_back();

private:
  WhiteboardDocument *m_document;  ///< Pointer to the document model
  ::PropertiesPanelModule *m_view; ///< Pointer to the properties panel view

  /**
   * \brief Apply a modification function to all selected shapes.
   * \param modifier Function that modifies a Stroke object
   *
   * This helper method:
   * - Gets the list of selected shape indices
   * - For each selected index:
   *   - Retrieves the stroke
   *   - Skips if locked
   *   - Applies the modifier function
   *   - Updates the stroke in the document
   *
   * The document's update_stroke() method handles:
   * - Undo stack management
   * - Observer notifications
   * - Redrawing
   */
  void apply_to_selected(std::function<void(Stroke &)> modifier);
};

} // namespace whiteboard

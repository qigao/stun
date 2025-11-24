/**
 * \file whiteboard_ddf_integration.h
 * \brief Integration layer between Whiteboard and DDF systems.
 */

#pragma once

#include "whiteboard/ddf/shape_layer.h"
#include "whiteboard/ddf/component_layer.h"
#include "whiteboard/types.h"
#include <nanogui/vector.h>
#include <memory>
#include <optional>
#include <string>

namespace whiteboard {

/**
 * \class WhiteboardDDFIntegration
 * \brief Converts between Whiteboard Stroke objects and DDF shapes.
 *
 * This class provides bidirectional conversion between the whiteboard's
 * native Stroke representation and DDF's Shape representation, enabling
 * seamless integration of DDF diagrams into the whiteboard application.
 */
class WhiteboardDDFIntegration {
public:
  WhiteboardDDFIntegration();
  ~WhiteboardDDFIntegration() = default;

  // === Stroke to DDF Conversion ===

  /**
   * \brief Convert a Stroke to a DDF Shape.
   * \param stroke The stroke to convert
   * \return DDF Shape, or nullopt if conversion not supported
   */
  std::optional<ddf::Shape> stroke_to_shape(const Stroke &stroke) const;

  /**
   * \brief Convert multiple Strokes to DDF Shapes.
   * \param strokes Vector of strokes to convert
   * \return Vector of DDF shapes
   */
  std::vector<ddf::Shape> strokes_to_shapes(const std::vector<Stroke> &strokes) const;

  // === DDF to Stroke Conversion ===

  /**
   * \brief Convert a DDF Shape to a Stroke.
   * \param shape The DDF shape to convert
   * \return Stroke representation
   */
  Stroke shape_to_stroke(const ddf::Shape &shape) const;

  /**
   * \brief Convert multiple DDF Shapes to Strokes.
   * \param shapes Vector of DDF shapes to convert
   * \return Vector of strokes
   */
  std::vector<Stroke> shapes_to_strokes(const std::vector<ddf::Shape> &shapes) const;

  // === Synchronization ===

  /**
   * \brief Sync a Stroke with its corresponding DDF Shape.
   * \param stroke The stroke to update
   * \param shape The DDF shape to sync from
   *
   * Updates the stroke's properties to match the DDF shape.
   */
  void sync_stroke_from_shape(Stroke &stroke, const ddf::Shape &shape) const;

  /**
   * \brief Sync a DDF Shape with its corresponding Stroke.
   * \param shape The DDF shape to update
   * \param stroke The stroke to sync from
   *
   * Updates the shape's properties to match the stroke.
   */
  void sync_shape_from_stroke(ddf::Shape &shape, const Stroke &stroke) const;

  // === Component Instance Conversion ===

  /**
   * \brief Convert a DDF ComponentInstance to Strokes.
   * \param instance The component instance
   * \param component_layer The component layer for looking up definitions
   * \return Vector of strokes representing the instantiated component
   */
  std::vector<Stroke> component_instance_to_strokes(
      const ddf::ComponentInstance &instance,
      ddf::ComponentLayer &component_layer) const;

private:
  // === Helper Methods ===

  /**
   * \brief Convert Tool enum to DDF shape type string.
   */
  std::string tool_to_shape_type(Tool tool) const;

  /**
   * \brief Convert DDF shape type string to Tool enum.
   */
  Tool shape_type_to_tool(const std::string &type) const;

  /**
   * \brief Extract geometry from stroke based on tool type.
   */
  std::map<std::string, float> extract_geometry(const Stroke &stroke) const;

  /**
   * \brief Apply geometry to stroke based on tool type.
   */
  void apply_geometry(Stroke &stroke, const std::map<std::string, float> &geometry) const;

  /**
   * \brief Convert NanoGUI Color to hex string.
   */
  std::string color_to_hex(const nanogui::Color &color) const;

  /**
   * \brief Convert hex string to NanoGUI Color.
   */
  nanogui::Color hex_to_color(const std::string &hex) const;

  /**
   * \brief Generate unique ID for DDF shapes.
   */
  std::string generate_shape_id() const;

  // ID counter for generating unique shape IDs
  mutable int m_next_shape_id = 1;
};

} // namespace whiteboard

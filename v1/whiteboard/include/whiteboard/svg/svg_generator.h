/**
 * \file svg_generator.h
 * \brief Generate SVG from DDF-like shape descriptions.
 */

#pragma once

#include <map>
#include <string>
#include <vector>

namespace whiteboard {

/**
 * \struct SVGShapeDesc
 * \brief DDF-like description of an SVG shape.
 */
struct SVGShapeDesc {
  std::string type;  // "rect", "circle", "ellipse", "path", "text", "group"
  std::map<std::string, float> geometry;  // x, y, width, height, cx, cy, rx, ry, r
  std::map<std::string, std::string> style;  // fill, stroke, stroke-width, opacity, etc.
  std::string text;  // For text elements
  std::string path_data;  // For path elements (d attribute)
  std::vector<SVGShapeDesc> children;  // For group elements
};

/**
 * \class SVGGenerator
 * \brief Generates SVG markup from DDF-like shape descriptions.
 *
 * This class allows creating SVG content using a simple JSON-like structure
 * similar to DDF, making it easier to programmatically generate SVG shapes
 * without writing raw XML/SVG markup.
 */
class SVGGenerator {
public:
  SVGGenerator();
  ~SVGGenerator() = default;

  /**
   * \brief Generate SVG document from shape description.
   * \param shape Root shape or group
   * \param width Document width (optional, auto-calculated if 0)
   * \param height Document height (optional, auto-calculated if 0)
   * \return Complete SVG document as string
   */
  std::string generate(const SVGShapeDesc& shape, float width = 0, float height = 0);

  /**
   * \brief Generate SVG from multiple shapes.
   * \param shapes Vector of shapes to include
   * \param width Document width (optional, auto-calculated if 0)
   * \param height Document height (optional, auto-calculated if 0)
   * \return Complete SVG document as string
   */
  std::string generate_multi(const std::vector<SVGShapeDesc>& shapes, 
                             float width = 0, float height = 0);

  /**
   * \brief Generate SVG element (without document wrapper).
   * \param shape Shape description
   * \return SVG element markup
   */
  std::string generate_element(const SVGShapeDesc& shape);

  /**
   * \brief Load shape description from JSON file (.svgshape).
   * \param filepath Path to .svgshape file
   * \return Shape description, or empty shape on error
   */
  static SVGShapeDesc load_from_file(const std::string& filepath);

  /**
   * \brief Load multiple shapes from JSON file (.svgshape).
   * \param filepath Path to .svgshape file
   * \return Vector of shape descriptions
   */
  static std::vector<SVGShapeDesc> load_multi_from_file(const std::string& filepath);

  /**
   * \brief Save shape description to JSON file (.svgshape).
   * \param shape Shape to save
   * \param filepath Path to save to
   * \return True if successful
   */
  static bool save_to_file(const SVGShapeDesc& shape, const std::string& filepath);

  /**
   * \brief Save multiple shapes to JSON file (.svgshape).
   * \param shapes Shapes to save
   * \param filepath Path to save to
   * \return True if successful
   */
  static bool save_multi_to_file(const std::vector<SVGShapeDesc>& shapes, 
                                 const std::string& filepath);

private:
  std::string generate_rect(const SVGShapeDesc& shape);
  std::string generate_circle(const SVGShapeDesc& shape);
  std::string generate_ellipse(const SVGShapeDesc& shape);
  std::string generate_path(const SVGShapeDesc& shape);
  std::string generate_text(const SVGShapeDesc& shape);
  std::string generate_group(const SVGShapeDesc& shape);
  
  std::string style_to_string(const std::map<std::string, std::string>& style);
  void calculate_bounds(const SVGShapeDesc& shape, float& min_x, float& min_y, 
                       float& max_x, float& max_y);
};

} // namespace whiteboard

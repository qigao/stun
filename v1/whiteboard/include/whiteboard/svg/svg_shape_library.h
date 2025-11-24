/**
 * \file svg_shape_library.h
 * \brief Shape library system for managing SVG shape templates.
 */

#pragma once

#include "whiteboard/types.h"
#include "whiteboard/svg/svg_generator.h"
#include <nlohmann/json.hpp>
#include <map>
#include <string>
#include <vector>

namespace whiteboard {

/**
 * \struct Parameter
 * \brief Defines an editable parameter for a shape.
 */
struct Parameter {
  std::string name;           // Parameter name (e.g., "className")
  std::string type;           // Parameter type: "text", "list", "number"
  std::string default_value;  // Default value
  std::string label;          // Display label for UI
  
  Parameter() = default;
  Parameter(const std::string& n, const std::string& t, const std::string& def, const std::string& lbl)
      : name(n), type(t), default_value(def), label(lbl) {}
};

/**
 * \struct ShapeDefinition
 * \brief Defines a shape template in the library.
 */
struct ShapeDefinition {
  std::string id;                      // Unique identifier (e.g., "uml.class")
  std::string name;                    // Display name (e.g., "Class")
  std::string category;                // Category (e.g., "UML", "Flowchart")
  std::string svg_file;                // Path to SVG file
  std::string thumbnail_file;          // Path to thumbnail image
  std::vector<Parameter> parameters;   // Editable parameters
  std::string description;             // Optional description
  
  ShapeDefinition() = default;
};

/**
 * \class SVGShapeLibrary
 * \brief Manages a library of SVG shape templates.
 *
 * The shape library loads shape definitions from a JSON file and provides
 * methods to query, filter, and instantiate shapes. It supports parametric
 * shapes where text and other properties can be customized.
 */
class SVGShapeLibrary {
public:
  SVGShapeLibrary();
  ~SVGShapeLibrary() = default;

  /**
   * \brief Load shape library from JSON file.
   * \param library_path Path to library.json file
   * \return True if successful, false on error
   *
   * Parses the library JSON and loads all shape definitions.
   * Validates that referenced SVG files exist.
   */
  bool load_library(const std::string& library_path);

  /**
   * \brief Get all unique categories in the library.
   * \return Vector of category names
   */
  std::vector<std::string> get_categories() const;

  /**
   * \brief Get all shapes in a specific category.
   * \param category Category name (empty string returns all shapes)
   * \return Vector of shape definitions
   */
  std::vector<ShapeDefinition> get_shapes(const std::string& category = "") const;

  /**
   * \brief Get a specific shape by ID.
   * \param shape_id Shape identifier
   * \return Pointer to shape definition, or nullptr if not found
   */
  const ShapeDefinition* get_shape(const std::string& shape_id) const;

  /**
   * \brief Create a stroke from a shape definition.
   * \param shape_id Shape identifier
   * \param position Position in canvas coordinates
   * \return Stroke object with SVG data
   *
   * Loads the SVG file, initializes parameters with defaults,
   * and creates a Stroke ready to be added to the document.
   */
  Stroke create_shape(const std::string& shape_id, const Point& position);

  /**
   * \brief Generate SVG with parameters applied.
   * \param shape_id Shape identifier
   * \param parameters Map of parameter values
   * \return SVG string with parameters substituted
   *
   * Takes an SVG template and replaces {{placeholder}} syntax
   * with actual parameter values. Handles text elements by ID.
   */
  std::string generate_svg(const std::string& shape_id, 
                           const std::map<std::string, std::string>& parameters);

  /**
   * \brief Check if library is loaded.
   * \return True if library has been successfully loaded
   */
  bool is_loaded() const { return m_loaded; }

  /**
   * \brief Get the base directory for shape files.
   * \return Base directory path
   */
  std::string get_base_directory() const { return m_base_directory; }

private:
  bool m_loaded;
  std::string m_base_directory;
  std::map<std::string, ShapeDefinition> m_shapes;

  /**
   * \brief Load SVG file content.
   * \param svg_path Path to SVG file
   * \return SVG content as string, or empty on error
   */
  std::string load_svg_file(const std::string& svg_path);

  /**
   * \brief Replace placeholders in SVG template.
   * \param svg_template SVG content with {{placeholders}}
   * \param parameters Parameter values
   * \return SVG with placeholders replaced
   */
  std::string replace_placeholders(const std::string& svg_template,
                                   const std::map<std::string, std::string>& parameters);

  /**
   * \brief Load and generate SVG from .svgshape file (DDF format).
   * \param filepath Path to .svgshape file
   * \return Generated SVG content
   */
  std::string load_svgshape_file(const std::string& filepath);

  /**
   * \brief Parse JSON into SVGShapeDesc structure.
   * \param j JSON object
   * \return Parsed shape description
   */
  SVGShapeDesc parse_svgshape_json(const nlohmann::json& j);

  /**
   * \brief Parse JSON array into vector of SVGShapeDesc.
   * \param j JSON array
   * \return Vector of parsed shape descriptions
   */
  std::vector<SVGShapeDesc> parse_svgshape_array(const nlohmann::json& j);
};

} // namespace whiteboard

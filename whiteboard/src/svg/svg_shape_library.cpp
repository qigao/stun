/**
 * \file svg_shape_library.cpp
 * \brief Implementation of SVG shape library system.
 */

#include "whiteboard/svg/svg_shape_library.h"
#include <algorithm>
#include <fmtlog.h>
#include <fstream>
#include <iostream>
#include <sstream>

namespace whiteboard {

SVGShapeLibrary::SVGShapeLibrary() : m_loaded(false) {}

bool SVGShapeLibrary::load_library(const std::string &library_path) {
  logi("SVGShapeLibrary: Loading library from '{}'", library_path);

  try {
    // Open and parse library JSON
    std::ifstream file(library_path);
    if (!file.is_open()) {
      loge("SVGShapeLibrary: Failed to open library file: '{}'", library_path);
      std::cerr << "Failed to open library file: " << library_path << std::endl;
      return false;
    }

    nlohmann::json j;
    file >> j;
    file.close();

    // Extract base directory from library path
    size_t pos = library_path.find_last_of("/\\");
    if (pos != std::string::npos) {
      m_base_directory = library_path.substr(0, pos + 1);
    } else {
      m_base_directory = "";
    }

    // Validate library format
    if (!j.contains("shapes") || !j["shapes"].is_array()) {
      std::cerr << "Invalid library format: missing 'shapes' array" << std::endl;
      return false;
    }

    // Load shape definitions
    m_shapes.clear();
    for (const auto &shape_json : j["shapes"]) {
      ShapeDefinition shape;

      // Required fields
      if (!shape_json.contains("id") || !shape_json.contains("name") ||
          !shape_json.contains("category") || !shape_json.contains("svg_file")) {
        std::cerr << "Shape definition missing required fields" << std::endl;
        continue;
      }

      shape.id = shape_json["id"].get<std::string>();
      shape.name = shape_json["name"].get<std::string>();
      shape.category = shape_json["category"].get<std::string>();
      shape.svg_file = shape_json["svg_file"].get<std::string>();

      // Optional fields
      shape.thumbnail_file = shape_json.value("thumbnail", "");
      shape.description = shape_json.value("description", "");

      // Load parameters
      if (shape_json.contains("parameters") && shape_json["parameters"].is_array()) {
        for (const auto &param_json : shape_json["parameters"]) {
          Parameter param;
          param.name = param_json.value("name", "");
          param.type = param_json.value("type", "text");
          param.default_value = param_json.value("default", "");
          param.label = param_json.value("label", param.name);
          shape.parameters.push_back(param);
        }
      }

      // Validate SVG file exists
      std::string svg_path = m_base_directory + shape.svg_file;
      std::ifstream svg_file(svg_path);
      if (!svg_file.good()) {
        std::cerr << "Warning: SVG file not found: " << svg_path << std::endl;
        // Continue loading anyway - might be created later
      }

      // Add to library
      m_shapes[shape.id] = shape;
    }

    m_loaded = true;
    std::cout << "Loaded " << m_shapes.size() << " shapes from library" << std::endl;
    
    // Log categories and shape counts
    auto categories = get_categories();
    logi("SVGShapeLibrary: Loaded {} categories:", categories.size());
    for (const auto& category : categories) {
      auto shapes = get_shapes(category);
      logi("  - {}: {} shapes", category, shapes.size());
      for (const auto& shape : shapes) {
        logd("    * {} ({}): {}", shape.name, shape.id, shape.svg_file);
      }
    }
    
    return true;

  } catch (const std::exception &e) {
    std::cerr << "Error loading shape library: " << e.what() << std::endl;
    return false;
  }
}

std::vector<std::string> SVGShapeLibrary::get_categories() const {
  std::vector<std::string> categories;

  for (const auto &pair : m_shapes) {
    const std::string &category = pair.second.category;
    if (std::find(categories.begin(), categories.end(), category) == categories.end()) {
      categories.push_back(category);
    }
  }

  // Sort alphabetically
  std::sort(categories.begin(), categories.end());

  return categories;
}

std::vector<ShapeDefinition> SVGShapeLibrary::get_shapes(const std::string &category) const {
  std::vector<ShapeDefinition> shapes;

  for (const auto &pair : m_shapes) {
    if (category.empty() || pair.second.category == category) {
      shapes.push_back(pair.second);
    }
  }

  return shapes;
}

const ShapeDefinition *SVGShapeLibrary::get_shape(const std::string &shape_id) const {
  auto it = m_shapes.find(shape_id);
  if (it != m_shapes.end()) {
    return &it->second;
  }
  return nullptr;
}

Stroke SVGShapeLibrary::create_shape(const std::string &shape_id, const Point &position) {
  logi("SVGShapeLibrary: Creating shape '{}' at ({}, {})", shape_id, position.x, position.y);

  Stroke stroke;
  stroke.tool = Tool::SVGShape;
  stroke.svg_shape_id = shape_id;
  stroke.svg_scale_x = 1.0f;
  stroke.svg_scale_y = 1.0f;
  stroke.rotation = 0.0f;

  // Set position
  stroke.points.push_back(position);

  // Get shape definition
  const ShapeDefinition *shape = get_shape(shape_id);
  if (!shape) {
    loge("SVGShapeLibrary: Shape not found: '{}'", shape_id);
    std::cerr << "Shape not found: " << shape_id << std::endl;
    return stroke;
  }

  logi("SVGShapeLibrary: Found shape definition - Name: '{}', SVG file: '{}'", shape->name,
       shape->svg_file);

  // Initialize parameters with defaults
  for (const auto &param : shape->parameters) {
    stroke.svg_parameters[param.name] = param.default_value;
  }

  // Load and generate SVG
  stroke.svg_data = generate_svg(shape_id, stroke.svg_parameters);

  if (stroke.svg_data.empty()) {
    loge("SVGShapeLibrary: Failed to generate SVG data for '{}'", shape_id);
  } else {
    logi("SVGShapeLibrary: Generated SVG data ({} bytes)", stroke.svg_data.size());
  }

  // Set stroke name
  stroke.name = shape->name;

  return stroke;
}

std::string SVGShapeLibrary::generate_svg(const std::string &shape_id,
                                          const std::map<std::string, std::string> &parameters) {
  logd("SVGShapeLibrary: Generating SVG for '{}'", shape_id);

  const ShapeDefinition *shape = get_shape(shape_id);
  if (!shape) {
    loge("SVGShapeLibrary: Shape definition not found for '{}'", shape_id);
    return "";
  }

  // Load SVG template
  std::string svg_path = m_base_directory + shape->svg_file;
  logd("SVGShapeLibrary: Loading SVG from '{}'", svg_path);

  std::string svg_template = load_svg_file(svg_path);
  if (svg_template.empty()) {
    loge("SVGShapeLibrary: Failed to load SVG file '{}'", svg_path);
    return "";
  }

  logd("SVGShapeLibrary: Loaded SVG template ({} bytes)", svg_template.size());

  // Replace placeholders
  return replace_placeholders(svg_template, parameters);
}

std::string SVGShapeLibrary::load_svg_file(const std::string &svg_path) {
  std::ifstream file(svg_path);
  if (!file.is_open()) {
    std::cerr << "Failed to open SVG file: " << svg_path << std::endl;
    return "";
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

std::string
SVGShapeLibrary::replace_placeholders(const std::string &svg_template,
                                      const std::map<std::string, std::string> &parameters) {
  std::string result = svg_template;

  // Replace {{placeholder}} syntax
  for (const auto &pair : parameters) {
    std::string placeholder = "{{" + pair.first + "}}";
    std::string value = pair.second;

    size_t pos = 0;
    while ((pos = result.find(placeholder, pos)) != std::string::npos) {
      result.replace(pos, placeholder.length(), value);
      pos += value.length();
    }
  }

  return result;
}

} // namespace whiteboard

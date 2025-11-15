/**
 * \file svg_generator.cpp
 * \brief Implementation of SVG generator from DDF-like descriptions.
 */

#include "whiteboard/svg/svg_generator.h"
#include <fmtlog.h>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <limits>

namespace whiteboard {

SVGGenerator::SVGGenerator() {
  logd("SVGGenerator: Initialized");
}

std::string SVGGenerator::generate(const SVGShapeDesc& shape, float width, float height) {
  logi("SVGGenerator: Generating SVG for shape type '{}'", shape.type);
  
  // Auto-calculate dimensions if not provided
  if (width <= 0 || height <= 0) {
    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();
    
    calculate_bounds(shape, min_x, min_y, max_x, max_y);
    
    if (width <= 0) width = max_x - min_x + 20;  // Add padding
    if (height <= 0) height = max_y - min_y + 20;
    
    logd("SVGGenerator: Auto-calculated dimensions: {}x{}", width, height);
  }
  
  std::ostringstream svg;
  svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" ";
  svg << "width=\"" << width << "\" height=\"" << height << "\" ";
  svg << "viewBox=\"0 0 " << width << " " << height << "\">\n";
  svg << generate_element(shape);
  svg << "</svg>";
  
  std::string result = svg.str();
  logi("SVGGenerator: Generated SVG document ({} bytes)", result.size());
  
  return result;
}

std::string SVGGenerator::generate_multi(const std::vector<SVGShapeDesc>& shapes, 
                                        float width, float height) {
  logi("SVGGenerator: Generating SVG for {} shapes", shapes.size());
  
  // Auto-calculate dimensions if not provided
  if (width <= 0 || height <= 0) {
    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();
    
    for (const auto& shape : shapes) {
      calculate_bounds(shape, min_x, min_y, max_x, max_y);
    }
    
    if (width <= 0) width = max_x - min_x + 20;
    if (height <= 0) height = max_y - min_y + 20;
    
    logd("SVGGenerator: Auto-calculated dimensions: {}x{}", width, height);
  }
  
  std::ostringstream svg;
  svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" ";
  svg << "width=\"" << width << "\" height=\"" << height << "\" ";
  svg << "viewBox=\"0 0 " << width << " " << height << "\">\n";
  
  for (const auto& shape : shapes) {
    svg << generate_element(shape);
  }
  
  svg << "</svg>";
  
  std::string result = svg.str();
  logi("SVGGenerator: Generated multi-shape SVG document ({} bytes)", result.size());
  
  return result;
}

std::string SVGGenerator::generate_element(const SVGShapeDesc& shape) {
  logd("SVGGenerator: Generating element for type '{}'", shape.type);
  
  if (shape.type == "rect") {
    return generate_rect(shape);
  } else if (shape.type == "circle") {
    return generate_circle(shape);
  } else if (shape.type == "ellipse") {
    return generate_ellipse(shape);
  } else if (shape.type == "path") {
    return generate_path(shape);
  } else if (shape.type == "text") {
    return generate_text(shape);
  } else if (shape.type == "group" || shape.type == "g") {
    return generate_group(shape);
  }
  
  logw("SVGGenerator: Unknown shape type '{}'", shape.type);
  return "";
}

std::string SVGGenerator::generate_rect(const SVGShapeDesc& shape) {
  std::ostringstream svg;
  svg << "  <rect ";
  
  // Geometry
  if (shape.geometry.count("x")) svg << "x=\"" << shape.geometry.at("x") << "\" ";
  if (shape.geometry.count("y")) svg << "y=\"" << shape.geometry.at("y") << "\" ";
  if (shape.geometry.count("width")) svg << "width=\"" << shape.geometry.at("width") << "\" ";
  if (shape.geometry.count("height")) svg << "height=\"" << shape.geometry.at("height") << "\" ";
  if (shape.geometry.count("rx")) svg << "rx=\"" << shape.geometry.at("rx") << "\" ";
  if (shape.geometry.count("ry")) svg << "ry=\"" << shape.geometry.at("ry") << "\" ";
  
  // Style
  if (!shape.style.empty()) {
    svg << "style=\"" << style_to_string(shape.style) << "\" ";
  }
  
  svg << "/>\n";
  return svg.str();
}

std::string SVGGenerator::generate_circle(const SVGShapeDesc& shape) {
  std::ostringstream svg;
  svg << "  <circle ";
  
  // Geometry
  if (shape.geometry.count("cx")) svg << "cx=\"" << shape.geometry.at("cx") << "\" ";
  if (shape.geometry.count("cy")) svg << "cy=\"" << shape.geometry.at("cy") << "\" ";
  if (shape.geometry.count("r")) svg << "r=\"" << shape.geometry.at("r") << "\" ";
  
  // Style
  if (!shape.style.empty()) {
    svg << "style=\"" << style_to_string(shape.style) << "\" ";
  }
  
  svg << "/>\n";
  return svg.str();
}

std::string SVGGenerator::generate_ellipse(const SVGShapeDesc& shape) {
  std::ostringstream svg;
  svg << "  <ellipse ";
  
  // Geometry
  if (shape.geometry.count("cx")) svg << "cx=\"" << shape.geometry.at("cx") << "\" ";
  if (shape.geometry.count("cy")) svg << "cy=\"" << shape.geometry.at("cy") << "\" ";
  if (shape.geometry.count("rx")) svg << "rx=\"" << shape.geometry.at("rx") << "\" ";
  if (shape.geometry.count("ry")) svg << "ry=\"" << shape.geometry.at("ry") << "\" ";
  
  // Style
  if (!shape.style.empty()) {
    svg << "style=\"" << style_to_string(shape.style) << "\" ";
  }
  
  svg << "/>\n";
  return svg.str();
}

std::string SVGGenerator::generate_path(const SVGShapeDesc& shape) {
  std::ostringstream svg;
  svg << "  <path ";
  
  // Path data
  if (!shape.path_data.empty()) {
    svg << "d=\"" << shape.path_data << "\" ";
  }
  
  // Style
  if (!shape.style.empty()) {
    svg << "style=\"" << style_to_string(shape.style) << "\" ";
  }
  
  svg << "/>\n";
  return svg.str();
}

std::string SVGGenerator::generate_text(const SVGShapeDesc& shape) {
  std::ostringstream svg;
  svg << "  <text ";
  
  // Geometry
  if (shape.geometry.count("x")) svg << "x=\"" << shape.geometry.at("x") << "\" ";
  if (shape.geometry.count("y")) svg << "y=\"" << shape.geometry.at("y") << "\" ";
  
  // Style
  if (!shape.style.empty()) {
    svg << "style=\"" << style_to_string(shape.style) << "\" ";
  }
  
  svg << ">";
  svg << shape.text;
  svg << "</text>\n";
  return svg.str();
}

std::string SVGGenerator::generate_group(const SVGShapeDesc& shape) {
  std::ostringstream svg;
  svg << "  <g";
  
  // Style for group
  if (!shape.style.empty()) {
    svg << " style=\"" << style_to_string(shape.style) << "\"";
  }
  
  svg << ">\n";
  
  // Generate children
  for (const auto& child : shape.children) {
    svg << generate_element(child);
  }
  
  svg << "  </g>\n";
  return svg.str();
}

std::string SVGGenerator::style_to_string(const std::map<std::string, std::string>& style) {
  std::ostringstream ss;
  bool first = true;
  
  for (const auto& [key, value] : style) {
    if (!first) ss << "; ";
    ss << key << ": " << value;
    first = false;
  }
  
  return ss.str();
}

void SVGGenerator::calculate_bounds(const SVGShapeDesc& shape, 
                                   float& min_x, float& min_y, 
                                   float& max_x, float& max_y) {
  if (shape.type == "rect") {
    if (shape.geometry.count("x") && shape.geometry.count("y") &&
        shape.geometry.count("width") && shape.geometry.count("height")) {
      float x = shape.geometry.at("x");
      float y = shape.geometry.at("y");
      float w = shape.geometry.at("width");
      float h = shape.geometry.at("height");
      
      min_x = std::min(min_x, x);
      min_y = std::min(min_y, y);
      max_x = std::max(max_x, x + w);
      max_y = std::max(max_y, y + h);
    }
  } else if (shape.type == "circle") {
    if (shape.geometry.count("cx") && shape.geometry.count("cy") && shape.geometry.count("r")) {
      float cx = shape.geometry.at("cx");
      float cy = shape.geometry.at("cy");
      float r = shape.geometry.at("r");
      
      min_x = std::min(min_x, cx - r);
      min_y = std::min(min_y, cy - r);
      max_x = std::max(max_x, cx + r);
      max_y = std::max(max_y, cy + r);
    }
  } else if (shape.type == "ellipse") {
    if (shape.geometry.count("cx") && shape.geometry.count("cy") &&
        shape.geometry.count("rx") && shape.geometry.count("ry")) {
      float cx = shape.geometry.at("cx");
      float cy = shape.geometry.at("cy");
      float rx = shape.geometry.at("rx");
      float ry = shape.geometry.at("ry");
      
      min_x = std::min(min_x, cx - rx);
      min_y = std::min(min_y, cy - ry);
      max_x = std::max(max_x, cx + rx);
      max_y = std::max(max_y, cy + ry);
    }
  } else if (shape.type == "text") {
    if (shape.geometry.count("x") && shape.geometry.count("y")) {
      float x = shape.geometry.at("x");
      float y = shape.geometry.at("y");
      
      // Approximate text bounds
      float w = shape.text.length() * 10.0f;
      float h = 20.0f;
      
      min_x = std::min(min_x, x);
      min_y = std::min(min_y, y - h);
      max_x = std::max(max_x, x + w);
      max_y = std::max(max_y, y);
    }
  } else if (shape.type == "group" || shape.type == "g") {
    for (const auto& child : shape.children) {
      calculate_bounds(child, min_x, min_y, max_x, max_y);
    }
  }
}

} // namespace whiteboard

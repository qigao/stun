/**
 * \file serialization.h
 * \brief Serialization utilities for whiteboard data structures
 */

#pragma once

#include "whiteboard/types.h"
#include <nlohmann/json.hpp>

namespace whiteboard {

using json = nlohmann::json;

/**
 * \brief Serialize a Stroke to JSON
 * \param stroke The stroke to serialize
 * \return JSON representation of the stroke
 */
json stroke_to_json(const Stroke& stroke);

/**
 * \brief Deserialize a Stroke from JSON
 * \param j The JSON object
 * \return Reconstructed stroke
 */
Stroke stroke_from_json(const json& j);

/**
 * \brief Serialize a Point to JSON array
 * \param p The point to serialize
 * \return JSON array [x, y]
 */
inline json point_to_json(const Point& p) {
  return json::array({p.x, p.y});
}

/**
 * \brief Deserialize a Point from JSON array
 * \param j The JSON array
 * \return Reconstructed point
 */
inline Point point_from_json(const json& j) {
  return Point(j[0].get<float>(), j[1].get<float>());
}

/**
 * \brief Serialize a Color to JSON array
 * \param c The color to serialize
 * \return JSON array [r, g, b, a]
 */
inline json color_to_json(const nanogui::Color& c) {
  return json::array({c.r(), c.g(), c.b(), c.w()});
}

/**
 * \brief Deserialize a Color from JSON array
 * \param j The JSON array
 * \return Reconstructed color
 */
inline nanogui::Color color_from_json(const json& j) {
  return nanogui::Color(j[0].get<float>(), j[1].get<float>(), 
                        j[2].get<float>(), j[3].get<float>());
}

} // namespace whiteboard

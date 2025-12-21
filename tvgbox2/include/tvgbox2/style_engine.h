/*
 * tvgbox2 - StyleEngine
 *
 * CSS parsing and rule matching engine (lexbor-powered)
 */

#ifndef TVGBOX2_STYLE_ENGINE_H
#define TVGBOX2_STYLE_ENGINE_H

#include "computed_style.h"
#include "element.h"
#include <string>
#include <memory>

namespace tvgbox2 {

/**
 * StyleEngine - CSS parsing and application (lexbor-powered)
 *
 * Features:
 * - Parse CSS strings using professional lexbor CSS parser
 * - Match selectors to elements
 * - Calculate specificity
 * - Apply cascade and inheritance
 * - Handle pseudo-classes dynamically
 * - Support for CSS variables (--*)
 * - Support for flexbox properties
 *
 * Example usage:
 * ```cpp
 * StyleEngine engine;
 * engine.parse_css(R"(
 *   button {
 *     --bg: 59,130,246,255;
 *     --text: 255,255,255,255;
 *   }
 *   button:hover {
 *     --bg: 29,78,216,255;
 *   }
 *   .primary {
 *     --bg: 34,197,94,255;
 *   }
 * )");
 *
 * engine.apply_styles(element);
 * ```
 */
class StyleEngine {
public:
  StyleEngine();
  ~StyleEngine();

  void parse_css(const std::string& css);
  void apply_styles(Element* elem);
  void clear();

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace tvgbox2

#endif // TVGBOX2_STYLE_ENGINE_H

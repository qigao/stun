/*
 * flexUI - StyleEngine
 *
 * CSS parsing and rule matching engine (lexbor-powered)
 */

#ifndef FLEXUI_STYLE_ENGINE_H
#define FLEXUI_STYLE_ENGINE_H

#include "computed_style.h"
#include "transition.h"
#include "element.h"
#include <string>
#include <memory>
#include <cstdint>
#include <vector>

namespace flexUI {

using StylesheetId = uint64_t;

enum class CssDiagnosticSeverity {
  Warning,
  Error,
};

struct CssDiagnostic {
  CssDiagnosticSeverity severity = CssDiagnosticSeverity::Warning;
  std::string source;
  size_t line = 0;
  size_t column = 0;
  std::string selector;
  std::string property;
  std::string value;
  std::string message;
};

struct CssLoadOptions {
  std::string source = "<inline>";
  // Strict mode rejects the whole load when any warning or error is found.
  bool strict = false;
};

struct CssLoadResult {
  StylesheetId stylesheet_id = 0;
  bool applied = false;
  std::vector<CssDiagnostic> diagnostics;

  bool has_errors() const;
};

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
  void append_css(const std::string& css);
  CssLoadResult load_stylesheet(const std::string& css,
                                const CssLoadOptions& options = {});
  CssLoadResult replace_stylesheet(StylesheetId stylesheet_id,
                                   const std::string& css,
                                   const CssLoadOptions& options = {});
  bool remove_stylesheet(StylesheetId stylesheet_id);
  void apply_styles(Element* elem);
  bool matches(const Element* elem, const std::string& selector) const;
  const std::vector<AnimationKeyframeStep>* keyframes(
      const std::string& name) const;
  bool uses_pseudo_class(Symbol pseudo) const;
  void clear();
  void clear_baseline_styles();

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace flexUI

#endif // FLEXUI_STYLE_ENGINE_H

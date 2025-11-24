#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace whiteboard {
namespace ddf {

/**
 * @brief Represents a CSS property-value pair
 */
struct CSSProperty {
  std::string name;
  std::string value;
};

/**
 * @brief Represents a parsed CSS rule
 */
struct CSSRule {
  std::string selector;
  std::vector<CSSProperty> properties;
};

/**
 * @brief Simple CSS parser for DDF stylesheets
 *
 * This is a basic CSS parser that handles the subset of CSS needed for DDF.
 * It supports:
 * - Type selectors (e.g., "rect", "circle")
 * - Class selectors (e.g., ".highlight")
 * - ID selectors (e.g., "#node1")
 * - Pseudo-class selectors (e.g., ":hover", ":selected")
 * - Descendant combinators (e.g., "group rect")
 *
 * Future: Can be replaced with a full CSS parser library if needed.
 */
class StyleSheetLoader {
public:
  StyleSheetLoader() = default;
  ~StyleSheetLoader() = default;

  /**
   * @brief Parse CSS text into a list of rules
   * @param css_text The CSS text to parse
   * @return Vector of parsed CSS rules
   */
  std::vector<CSSRule> parse(const std::string &css_text);

  /**
   * @brief Load CSS from a file
   * @param file_path Path to the CSS file
   * @return Vector of parsed CSS rules
   */
  std::vector<CSSRule> load_from_file(const std::string &file_path);

private:
  // Helper methods for parsing
  std::string trim(const std::string &str);
  std::vector<std::string> split_selectors(const std::string &selector_text);
  std::vector<CSSProperty> parse_properties(const std::string &properties_text);
  CSSProperty parse_property(const std::string &property_text);
};

} // namespace ddf
} // namespace whiteboard

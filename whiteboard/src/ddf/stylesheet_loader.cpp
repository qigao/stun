#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <whiteboard/ddf/stylesheet_loader.h>

namespace whiteboard {
namespace ddf {

std::string StyleSheetLoader::trim(const std::string &str) {
  auto start = str.begin();
  while (start != str.end() && std::isspace(*start)) {
    start++;
  }

  auto end = str.end();
  do {
    end--;
  } while (std::distance(start, end) > 0 && std::isspace(*end));

  return std::string(start, end + 1);
}

std::vector<std::string> StyleSheetLoader::split_selectors(const std::string &selector_text) {
  std::vector<std::string> selectors;
  std::stringstream ss(selector_text);
  std::string selector;

  while (std::getline(ss, selector, ',')) {
    selector = trim(selector);
    if (!selector.empty()) {
      selectors.push_back(selector);
    }
  }

  return selectors;
}

CSSProperty StyleSheetLoader::parse_property(const std::string &property_text) {
  CSSProperty prop;
  size_t colon_pos = property_text.find(':');

  if (colon_pos != std::string::npos) {
    prop.name = trim(property_text.substr(0, colon_pos));
    prop.value = trim(property_text.substr(colon_pos + 1));

    // Remove trailing semicolon if present
    if (!prop.value.empty() && prop.value.back() == ';') {
      prop.value.pop_back();
      prop.value = trim(prop.value);
    }
  }

  return prop;
}

std::vector<CSSProperty> StyleSheetLoader::parse_properties(const std::string &properties_text) {
  std::vector<CSSProperty> properties;
  std::stringstream ss(properties_text);
  std::string property_text;

  while (std::getline(ss, property_text, ';')) {
    property_text = trim(property_text);
    if (!property_text.empty()) {
      CSSProperty prop = parse_property(property_text);
      if (!prop.name.empty()) {
        properties.push_back(prop);
      }
    }
  }

  return properties;
}

std::vector<CSSRule> StyleSheetLoader::parse(const std::string &css_text) {
  std::vector<CSSRule> rules;

  size_t pos = 0;
  while (pos < css_text.length()) {
    // Skip whitespace and comments
    while (pos < css_text.length() && std::isspace(css_text[pos])) {
      pos++;
    }

    // Skip CSS comments /* ... */
    if (pos + 1 < css_text.length() && css_text[pos] == '/' && css_text[pos + 1] == '*') {
      pos = css_text.find("*/", pos + 2);
      if (pos == std::string::npos) {
        break;
      }
      pos += 2;
      continue;
    }

    if (pos >= css_text.length()) {
      break;
    }

    // Find selector (everything before '{')
    size_t brace_start = css_text.find('{', pos);
    if (brace_start == std::string::npos) {
      break;
    }

    std::string selector_text = css_text.substr(pos, brace_start - pos);
    selector_text = trim(selector_text);

    // Find properties (everything between '{' and '}')
    size_t brace_end = css_text.find('}', brace_start);
    if (brace_end == std::string::npos) {
      break;
    }

    std::string properties_text = css_text.substr(brace_start + 1, brace_end - brace_start - 1);

    // Parse properties
    std::vector<CSSProperty> properties = parse_properties(properties_text);

    // Split multiple selectors (e.g., "rect, circle { ... }")
    std::vector<std::string> selectors = split_selectors(selector_text);

    // Create a rule for each selector
    for (const auto &selector : selectors) {
      CSSRule rule;
      rule.selector = selector;
      rule.properties = properties;
      rules.push_back(rule);
    }

    pos = brace_end + 1;
  }

  return rules;
}

std::vector<CSSRule> StyleSheetLoader::load_from_file(const std::string &file_path) {
  std::ifstream file(file_path);
  if (!file.is_open()) {
    return {};
  }

  std::stringstream buffer;
  buffer << file.rdbuf();

  return parse(buffer.str());
}

} // namespace ddf
} // namespace whiteboard

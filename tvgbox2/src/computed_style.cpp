/*
 * tvgbox2 - ComputedStyle Implementation
 */

#include <tvgbox2/computed_style.h>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <iostream>

namespace tvgbox2 {

// Helper to update result string by replacing range
static void replace_all(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
}

// Helper to trim string
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (std::string::npos == first) return str;
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

std::string ComputedStyle::resolve_variable_value(const std::string& value, int depth) const {
    if (depth > 10) return value; // Prevent recursion limit
    
    std::string result = value;
    size_t var_pos = 0;
    
    // Iterate over all var() occurrences
    // Note: This simple parser handles "var(--a)" but assumes no nested parenthesis inside fallback unless careful.
    // For robust CSS parsing we might need more, but for this demo:
    while ((var_pos = result.find("var(", var_pos)) != std::string::npos) {
        size_t open_paren = var_pos + 4;
        size_t close_paren = result.find(")", open_paren);
        
        // Handle nested parentheses in fallback (e.g. var(--a, var(--b)))
        // Simple counter approach
        int paren_depth = 1;
        size_t current = open_paren;
        size_t found_close = std::string::npos;
        
        while (current < result.size()) {
            if (result[current] == '(') paren_depth++;
            else if (result[current] == ')') paren_depth--;
            
            if (paren_depth == 0) {
                found_close = current;
                break;
            }
            current++;
        }
        
        if (found_close == std::string::npos) break; // Malformed
        
        std::string inner = result.substr(open_paren, found_close - open_paren);
        
        // Parse name and fallback
        std::string name;
        std::string fallback;
        
        size_t comma = inner.find(',');
        if (comma != std::string::npos) {
            name = trim(inner.substr(0, comma));
            fallback = trim(inner.substr(comma + 1));
        } else {
            name = trim(inner);
        }
        
        // Resolve value
        std::string resolved_val;
        bool found = false;
        
        // Look up variable
        auto it = variables.find(name);
        if (it != variables.end()) {
            resolved_val = resolve_variable_value(it->second, depth + 1);
            found = true;
        } else if (!fallback.empty()) {
            resolved_val = resolve_variable_value(fallback, depth + 1);
            found = true;
        }
        
        if (found) {
            result.replace(var_pos, found_close - var_pos + 1, resolved_val);
            // Don't advance var_pos, we might need to resolve what we just replaced if it contained vars 
            // (though we already recursively resolved, so we should be good, but safe to stay or advance carefully)
            // But if resolved_val contains "var(", infinite loop?
            // resolve_variable_value is recursive, so resolved_val should be fully resolved.
            // So we can advance.
            var_pos += resolved_val.length();
        } else {
             // Not found and no fallback, leave as is or empty? CSS says invalid at computed value time usually triggers unset/initial.
             // We'll leave it empty to avoid garbage parsing
             result.replace(var_pos, found_close - var_pos + 1, "");
        }
    }
    
    return result;
}

std::string ComputedStyle::get_variable(const std::string& name, const std::string& default_value) const {
    auto it = variables.find(name);
    if (it == variables.end()) return default_value;
    return resolve_variable_value(it->second);
}

Color ComputedStyle::get_variable_color(const std::string& name,
                                        const Color& default_color) const {
  auto it = variables.find(name);
  if (it == variables.end()) {
    return default_color;
  }

  // Resolve var() references
  std::string value = resolve_variable_value(it->second);
  if (value.empty()) return default_color;

  Color result = default_color;

  // 使用 stringstream 解析
  std::istringstream ss(value);
  std::string token;
  int index = 0;

  // Check if it looks like r,g,b,a
  // Variables are stored as "r, g, b, a" strings (0-255 range)
  // Convert to float (0.0-1.0) for flex::Color

  while (std::getline(ss, token, ',') && index < 4) {
    int component = std::atoi(token.c_str());
    component = std::max(0, std::min(255, component)); // clamp to [0, 255]
    float normalized = component / 255.0f;

    switch (index) {
      case 0: result.r = normalized; break;
      case 1: result.g = normalized; break;
      case 2: result.b = normalized; break;
      case 3: result.a = normalized; break;
    }
    index++;
  }

  return result;
}

float ComputedStyle::get_variable_float(const std::string& name, float default_value) const {
    auto it = variables.find(name);
    if (it == variables.end()) return default_value;

    std::string value = resolve_variable_value(it->second);
    if (value.empty()) return default_value;
    
    return std::strtof(value.c_str(), nullptr);
}

} // namespace tvgbox2

#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <map>

namespace flexui {
namespace xml_utils {

// Parse comma-separated string into vector
inline std::vector<std::string> parse_comma_separated(const std::string& str) {
    std::vector<std::string> result;
    std::istringstream ss(str);
    std::string item;
    while (std::getline(ss, item, ',')) {
        // Trim whitespace
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    return result;
}

// Parse enum from string with mapping
template<typename EnumType>
EnumType parse_enum(const std::string& str, 
                    const std::map<std::string, EnumType>& mapping, 
                    EnumType default_value) {
    auto it = mapping.find(str);
    return (it != mapping.end()) ? it->second : default_value;
}

// Parse inline CSS style string into property-value pairs
inline std::map<std::string, std::string> parse_inline_style(const std::string& style_str) {
    std::map<std::string, std::string> styles;
    size_t pos = 0;
    
    while (pos < style_str.length()) {
        size_t colon_pos = style_str.find(':', pos);
        if (colon_pos == std::string::npos) break;
        
        size_t semicolon_pos = style_str.find(';', colon_pos);
        if (semicolon_pos == std::string::npos) semicolon_pos = style_str.length();
        
        std::string property = style_str.substr(pos, colon_pos - pos);
        std::string value = style_str.substr(colon_pos + 1, semicolon_pos - colon_pos - 1);
        
        // Trim whitespace
        property.erase(0, property.find_first_not_of(" \t"));
        property.erase(property.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        if (!property.empty() && !value.empty()) {
            styles[property] = value;
        }
        
        pos = semicolon_pos + 1;
    }
    
    return styles;
}

} // namespace xml_utils
} // namespace flexui

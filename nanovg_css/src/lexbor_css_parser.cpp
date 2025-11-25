#include "lexbor_css_parser.h"
#include "nanovg_css_internal.h"
#include "nanovg_css_types.h"
#include "nanovg_css_conversion.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <memory>

namespace nanovg_css {
namespace lexbor {

// ============================================================================
// LexborCSSParser Implementation
// ============================================================================

bool LexborCSSParser::parse(const std::string& css) {
    clear();
    
    try {
        parser_ = std::make_unique<CSSParser>();
        lxb_css_stylesheet_t* sheet = parser_->parse(css);
        stylesheet_ = std::make_unique<CSSStyleSheet>(sheet);
        return true;
    } catch (const std::exception& e) {
        errors_.push_back(std::string("CSS parsing error: ") + e.what());
        return false;
    }
}

void LexborCSSParser::clear() {
    parser_.reset();
    stylesheet_.reset();
    errors_.clear();
}

// ============================================================================
// LexborSelectorMatcher Implementation
// ============================================================================

bool LexborSelectorMatcher::matches(
    const std::string& selector,
    const std::string& shape_id,
    const std::string& shape_type,
    const std::vector<std::string>& classes,
    const std::map<std::string, std::string>& attributes,
    const std::set<std::string>& pseudo_states) const {
    
    // Check if selector contains combinators
    if (selector.find('>') != std::string::npos ||
        selector.find('+') != std::string::npos ||
        selector.find('~') != std::string::npos ||
        (selector.find(' ') != std::string::npos && selector.find(' ') != selector.find_last_not_of(" \t\n\r") + 1)) {
        // Complex selector with combinators
        // For now, just match the last component (rightmost selector)
        // Full combinator support requires shape hierarchy
        auto complex = parse_complex_selector(selector);
        if (!complex.components.empty()) {
            return matches_simple_selector(complex.components.back(), shape_id, shape_type, classes, attributes, pseudo_states);
        }
        return false;
    }
    
    // Simple selector - parse and match
    auto sel = parse_selector(selector);
    return matches_simple_selector(sel, shape_id, shape_type, classes, attributes, pseudo_states);
}

int LexborSelectorMatcher::calculate_specificity(const std::string& selector) const {
    auto sel = parse_selector(selector);
    
    int specificity = 0;
    
    // ID selector: 100
    if (!sel.id.empty()) {
        specificity += 100;
    }
    
    // Class selectors: 10 each
    specificity += static_cast<int>(sel.classes.size()) * 10;
    
    // Attribute selectors: 10 each
    specificity += static_cast<int>(sel.attributes.size()) * 10;
    
    // Pseudo-classes: 10 each
    specificity += static_cast<int>(sel.pseudo_classes.size()) * 10;
    
    // Type selector: 1
    if (!sel.type.empty() && sel.type != "*") {
        specificity += 1;
    }
    
    return specificity;
}

LexborSelectorMatcher::SelectorComponents 
LexborSelectorMatcher::parse_selector(const std::string& selector) const {
    SelectorComponents result;
    
    std::string trimmed = selector;
    // Trim whitespace
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
    
    size_t i = 0;
    while (i < trimmed.size()) {
        char c = trimmed[i];
        
        if (c == '#') {
            // ID selector
            i++;
            size_t start = i;
            while (i < trimmed.size() && (std::isalnum(trimmed[i]) || trimmed[i] == '_' || trimmed[i] == '-')) {
                i++;
            }
            result.id = trimmed.substr(start, i - start);
        }
        else if (c == '.') {
            // Class selector
            i++;
            size_t start = i;
            while (i < trimmed.size() && (std::isalnum(trimmed[i]) || trimmed[i] == '_' || trimmed[i] == '-')) {
                i++;
            }
            result.classes.push_back(trimmed.substr(start, i - start));
        }
        else if (c == '[') {
            // Attribute selector
            i++;
            size_t start = i;
            while (i < trimmed.size() && trimmed[i] != ']') {
                i++;
            }
            std::string attr_expr = trimmed.substr(start, i - start);
            
            // Trim whitespace from attribute expression
            attr_expr.erase(0, attr_expr.find_first_not_of(" \t"));
            attr_expr.erase(attr_expr.find_last_not_of(" \t") + 1);
            
            // Parse attribute expression with operators
            // Supported: [attr], [attr=value], [attr^=value], [attr$=value], [attr*=value], [attr~=value], [attr|=value]
            std::string attr_name;
            std::string attr_operator = "="; // Default to exact match
            std::string attr_value;
            
            // Check for operators
            size_t op_pos = std::string::npos;
            if ((op_pos = attr_expr.find("^=")) != std::string::npos) {
                attr_operator = "^=";
                attr_name = attr_expr.substr(0, op_pos);
                attr_value = attr_expr.substr(op_pos + 2);
            } else if ((op_pos = attr_expr.find("$=")) != std::string::npos) {
                attr_operator = "$=";
                attr_name = attr_expr.substr(0, op_pos);
                attr_value = attr_expr.substr(op_pos + 2);
            } else if ((op_pos = attr_expr.find("*=")) != std::string::npos) {
                attr_operator = "*=";
                attr_name = attr_expr.substr(0, op_pos);
                attr_value = attr_expr.substr(op_pos + 2);
            } else if ((op_pos = attr_expr.find("~=")) != std::string::npos) {
                attr_operator = "~=";
                attr_name = attr_expr.substr(0, op_pos);
                attr_value = attr_expr.substr(op_pos + 2);
            } else if ((op_pos = attr_expr.find("|=")) != std::string::npos) {
                attr_operator = "|=";
                attr_name = attr_expr.substr(0, op_pos);
                attr_value = attr_expr.substr(op_pos + 2);
            } else if ((op_pos = attr_expr.find('=')) != std::string::npos) {
                attr_operator = "=";
                attr_name = attr_expr.substr(0, op_pos);
                attr_value = attr_expr.substr(op_pos + 1);
            } else {
                // Just [attr] - attribute exists
                attr_name = attr_expr;
                attr_operator = "exists";
                attr_value = "";
            }
            
            // Trim attribute name and value
            attr_name.erase(0, attr_name.find_first_not_of(" \t"));
            attr_name.erase(attr_name.find_last_not_of(" \t") + 1);
            attr_value.erase(0, attr_value.find_first_not_of(" \t"));
            attr_value.erase(attr_value.find_last_not_of(" \t") + 1);
            
            // Remove quotes from value
            if (!attr_value.empty() && (attr_value.front() == '"' || attr_value.front() == '\'')) {
                attr_value = attr_value.substr(1, attr_value.size() - 2);
            }
            
            // Store with operator prefix
            result.attributes[attr_operator + ":" + attr_name] = attr_value;
            
            if (i < trimmed.size()) i++; // Skip ]
        }
        else if (c == ':') {
            // Pseudo-class
            i++;
            size_t start = i;
            while (i < trimmed.size() && (std::isalnum(trimmed[i]) || trimmed[i] == '_' || trimmed[i] == '-')) {
                i++;
            }
            result.pseudo_classes.push_back(trimmed.substr(start, i - start));
        }
        else if (std::isalpha(c) || c == '*') {
            // Type selector
            size_t start = i;
            while (i < trimmed.size() && (std::isalnum(trimmed[i]) || trimmed[i] == '_' || trimmed[i] == '-' || trimmed[i] == '*')) {
                i++;
            }
            result.type = trimmed.substr(start, i - start);
        }
        else {
            i++;
        }
    }
    
    return result;
}

LexborSelectorMatcher::ComplexSelector 
LexborSelectorMatcher::parse_complex_selector(const std::string& selector) const {
    ComplexSelector result;
    
    std::string trimmed = selector;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
    
    std::string current_selector;
    size_t i = 0;
    
    while (i < trimmed.size()) {
        char c = trimmed[i];
        
        // Check for combinators
        if (c == '>' || c == '+' || c == '~') {
            // Parse the selector before the combinator
            if (!current_selector.empty()) {
                result.components.push_back(parse_selector(current_selector));
                current_selector.clear();
            }
            result.combinators.push_back(c);
            i++;
            // Skip whitespace after combinator
            while (i < trimmed.size() && (trimmed[i] == ' ' || trimmed[i] == '\t')) {
                i++;
            }
        }
        else if (c == ' ' || c == '\t') {
            // Descendant combinator (space)
            // Skip multiple spaces
            while (i < trimmed.size() && (trimmed[i] == ' ' || trimmed[i] == '\t')) {
                i++;
            }
            // Only add descendant combinator if there's more content
            if (i < trimmed.size() && !current_selector.empty()) {
                result.components.push_back(parse_selector(current_selector));
                current_selector.clear();
                result.combinators.push_back(' ');
            }
        }
        else {
            current_selector += c;
            i++;
        }
    }
    
    // Add the last selector
    if (!current_selector.empty()) {
        result.components.push_back(parse_selector(current_selector));
    }
    
    return result;
}

bool LexborSelectorMatcher::matches_simple_selector(
    const SelectorComponents& sel,
    const std::string& shape_id,
    const std::string& shape_type,
    const std::vector<std::string>& classes,
    const std::map<std::string, std::string>& attributes,
    const std::set<std::string>& pseudo_states) const {
    
    // Check ID
    if (!sel.id.empty() && sel.id != shape_id) {
        return false;
    }
    
    // Check type
    if (!sel.type.empty() && sel.type != "*" && sel.type != shape_type) {
        return false;
    }
    
    // Check classes
    for (const auto& cls : sel.classes) {
        if (std::find(classes.begin(), classes.end(), cls) == classes.end()) {
            return false;
        }
    }
    
    // Check attributes
    for (const auto& [attr_key, attr_value] : sel.attributes) {
        // Parse operator and attribute name from key (format: "operator:attr_name")
        size_t colon_pos = attr_key.find(':');
        if (colon_pos == std::string::npos) {
            // Old format without operator prefix - treat as exact match
            auto it = attributes.find(attr_key);
            if (it == attributes.end() || it->second != attr_value) {
                return false;
            }
            continue;
        }
        
        std::string op = attr_key.substr(0, colon_pos);
        std::string attr_name = attr_key.substr(colon_pos + 1);
        
        auto it = attributes.find(attr_name);
        
        if (op == "exists") {
            // [attr] - just check if attribute exists
            if (it == attributes.end()) {
                return false;
            }
        } else if (op == "=") {
            // [attr=value] - exact match
            if (it == attributes.end() || it->second != attr_value) {
                return false;
            }
        } else if (op == "^=") {
            // [attr^=value] - starts with
            if (it == attributes.end() || it->second.find(attr_value) != 0) {
                return false;
            }
        } else if (op == "$=") {
            // [attr$=value] - ends with
            if (it == attributes.end() || 
                it->second.size() < attr_value.size() ||
                it->second.substr(it->second.size() - attr_value.size()) != attr_value) {
                return false;
            }
        } else if (op == "*=") {
            // [attr*=value] - contains
            if (it == attributes.end() || it->second.find(attr_value) == std::string::npos) {
                return false;
            }
        } else if (op == "~=") {
            // [attr~=value] - word match (space-separated)
            if (it == attributes.end()) {
                return false;
            }
            // Check if attr_value appears as a complete word in the attribute value
            // Words are separated by spaces
            const std::string& value_str = it->second;
            bool found = false;
            
            // Check if it's the only word
            if (value_str == attr_value) {
                found = true;
            } else {
                // Check if it's at the start followed by space
                if (value_str.find(attr_value + " ") == 0) {
                    found = true;
                }
                // Check if it's at the end preceded by space
                else if (value_str.size() >= attr_value.size() + 1 &&
                         value_str.substr(value_str.size() - attr_value.size() - 1) == " " + attr_value) {
                    found = true;
                }
                // Check if it's in the middle surrounded by spaces
                else if (value_str.find(" " + attr_value + " ") != std::string::npos) {
                    found = true;
                }
            }
            
            if (!found) {
                return false;
            }
        } else if (op == "|=") {
            // [attr|=value] - prefix match (value or value-)
            if (it == attributes.end()) {
                return false;
            }
            if (it->second != attr_value && 
                it->second.find(attr_value + "-") != 0) {
                return false;
            }
        }
    }
    
    // Check pseudo-classes
    for (const auto& pseudo : sel.pseudo_classes) {
        if (pseudo_states.find(pseudo) == pseudo_states.end()) {
            return false;
        }
    }
    
    return true;
}

bool LexborSelectorMatcher::matches_with_hierarchy(
    const std::string& selector,
    const std::string& shape_id,
    const std::string& shape_type,
    const std::vector<std::string>& classes,
    const std::map<std::string, std::string>& attributes,
    const std::set<std::string>& pseudo_states,
    const std::string& parent_shape_id,
    std::function<const void*(const std::string&)> get_shape_func) const {
    
    // Parse complex selector
    auto complex = parse_complex_selector(selector);
    
    // If no combinators, just match simple selector
    if (complex.combinators.empty()) {
        if (!complex.components.empty()) {
            return matches_simple_selector(complex.components[0], shape_id, shape_type, classes, attributes, pseudo_states);
        }
        return false;
    }
    
    // TODO: Implement full combinator matching with shape hierarchy
    // For now, just match the rightmost selector (the target element)
    if (!complex.components.empty()) {
        return matches_simple_selector(complex.components.back(), shape_id, shape_type, classes, attributes, pseudo_states);
    }
    
    return false;
}

// ============================================================================
// LexborStyleComputer Implementation
// ============================================================================

std::map<std::string, std::string> LexborStyleComputer::compute_style(
    lxb_css_stylesheet_t* stylesheet,
    const std::vector<CSSRule>& rules,
    const std::string& shape_id,
    const std::string& shape_type,
    const std::vector<std::string>& classes,
    const std::map<std::string, std::string>& attributes,
    const std::set<std::string>& pseudo_states,
    const std::map<std::string, std::string>& inline_style,
    const std::map<std::string, std::string>& parent_style) const {

    // 1. Start with default styles
    std::map<std::string, std::string> result = get_default_styles(shape_type);

    // 2. Apply inheritance
    apply_inheritance(result, parent_style);

    // 3. Apply stylesheet rules (NOW IMPLEMENTED!)
    // Collect matching rules with their specificity
    std::vector<std::pair<int, const CSSRule*>> matching_rules;

    for (const auto& rule : rules) {
        // Check if this rule's selector matches the element
        if (matcher_.matches(rule.selector, shape_id, shape_type, classes, attributes, pseudo_states)) {
            matching_rules.push_back({rule.specificity, &rule});
        }
    }

    // Sort by specificity (lowest to highest, so higher specificity overwrites)
    std::sort(matching_rules.begin(), matching_rules.end(),
        [](const auto& a, const auto& b) {
            return a.first < b.first;
        });

    // Apply matching rules in order of specificity
    for (const auto& [spec, rule_ptr] : matching_rules) {
        for (const auto& [key, value] : rule_ptr->properties) {
            result[key] = value;
        }
    }

    // 4. Apply inline styles (highest priority)
    for (const auto& [key, value] : inline_style) {
        result[key] = value;
    }

    return result;
}

bool LexborStyleComputer::is_inheritable(const std::string& property) const {
    static const std::set<std::string> inheritable = {
        "color",
        "font-family",
        "font-size",
        "font-weight",
        "font-style",
        "text-align",
        "line-height",
        "opacity"
    };
    
    return inheritable.count(property) > 0;
}

std::map<std::string, std::string>
LexborStyleComputer::get_default_styles(const std::string& shape_type) const {
    std::map<std::string, std::string> defaults;

    // Common defaults (CSS properties for nanovg_css)
    // CSS default: background is transparent (not inherited)
    defaults["background"] = "transparent";
    defaults["color"] = "black";
    defaults["opacity"] = "1";

    // SVG-style properties (for compatibility)
    defaults["fill"] = "none";
    defaults["stroke"] = "black";
    defaults["stroke-width"] = "1";

    // Type-specific defaults
    if (shape_type == "text") {
        defaults["font-family"] = "sans-serif";
        defaults["font-size"] = "14";
        defaults["font-weight"] = "normal";
        defaults["text-align"] = "left";
        defaults["color"] = "black";
        defaults["fill"] = "black";  // SVG-style
        defaults["background"] = "transparent";  // Text has no background by default
    } else if (shape_type == "rect" || shape_type == "circle" ||
               shape_type == "ellipse" || shape_type == "path") {
        // SVG shapes default to gray fill (SVG-style)
        defaults["fill"] = "#cccccc";
        // But CSS background should still be transparent unless specified
    }

    return defaults;
}

void LexborStyleComputer::apply_inheritance(
    std::map<std::string, std::string>& result,
    const std::map<std::string, std::string>& parent_style) const {
    
    for (const auto& [key, value] : parent_style) {
        if (is_inheritable(key)) {
            result[key] = value;
        }
    }
}

// ============================================================================
// EnhancedStyleSheet Implementation
// ============================================================================

bool EnhancedStyleSheet::parse_css(const std::string& css) {
    clear_cache();

    // Parse with Lexbor (for validation and internal structures)
    bool success = parser_.parse(css);

    // Extract rules manually (since Lexbor doesn't expose them)
    // Simple CSS parser to extract selector { prop: value; ... } blocks
    size_t pos = 0;
    while (pos < css.size()) {
        // Skip whitespace and comments
        while (pos < css.size() && (css[pos] == ' ' || css[pos] == '\t' || css[pos] == '\n' || css[pos] == '\r')) {
            pos++;
        }

        if (pos >= css.size()) break;

        // Skip comments /* ... */
        if (pos + 1 < css.size() && css[pos] == '/' && css[pos + 1] == '*') {
            pos += 2;
            while (pos + 1 < css.size()) {
                if (css[pos] == '*' && css[pos + 1] == '/') {
                    pos += 2;
                    break;
                }
                pos++;
            }
            continue;
        }

        // Find selector (everything before '{')
        size_t selector_start = pos;
        while (pos < css.size() && css[pos] != '{') {
            pos++;
        }

        if (pos >= css.size()) break;  // No more rules

        std::string selector = css.substr(selector_start, pos - selector_start);

        // Trim selector
        size_t sel_start = selector.find_first_not_of(" \t\n\r");
        size_t sel_end = selector.find_last_not_of(" \t\n\r");
        if (sel_start != std::string::npos) {
            selector = selector.substr(sel_start, sel_end - sel_start + 1);
        }

        pos++;  // Skip '{'

        // Find properties block (everything before '}')
        size_t props_start = pos;
        int brace_count = 1;
        while (pos < css.size() && brace_count > 0) {
            if (css[pos] == '{') brace_count++;
            else if (css[pos] == '}') brace_count--;
            if (brace_count > 0) pos++;
        }

        std::string props_block = css.substr(props_start, pos - props_start);
        pos++;  // Skip '}'

        // Parse properties
        std::map<std::string, std::string> properties;
        size_t prop_pos = 0;
        while (prop_pos < props_block.size()) {
            // Skip whitespace
            while (prop_pos < props_block.size() && (props_block[prop_pos] == ' ' || props_block[prop_pos] == '\t' || props_block[prop_pos] == '\n' || props_block[prop_pos] == '\r')) {
                prop_pos++;
            }

            if (prop_pos >= props_block.size()) break;

            // Find property name (before ':')
            size_t prop_name_start = prop_pos;
            while (prop_pos < props_block.size() && props_block[prop_pos] != ':') {
                prop_pos++;
            }

            if (prop_pos >= props_block.size()) break;

            std::string prop_name = props_block.substr(prop_name_start, prop_pos - prop_name_start);

            // Trim property name
            size_t pn_start = prop_name.find_first_not_of(" \t\n\r");
            size_t pn_end = prop_name.find_last_not_of(" \t\n\r");
            if (pn_start != std::string::npos) {
                prop_name = prop_name.substr(pn_start, pn_end - pn_start + 1);
            }

            // Normalize property name to lowercase
            std::transform(prop_name.begin(), prop_name.end(), prop_name.begin(),
                           [](unsigned char c){ return std::tolower(c); });

            prop_pos++;  // Skip ':'

            // Find property value (before ';' or '}')
            size_t prop_value_start = prop_pos;
            while (prop_pos < props_block.size() && props_block[prop_pos] != ';' && props_block[prop_pos] != '}') {
                prop_pos++;
            }

            std::string prop_value = props_block.substr(prop_value_start, prop_pos - prop_value_start);

            // Trim property value
            size_t pv_start = prop_value.find_first_not_of(" \t\n\r");
            size_t pv_end = prop_value.find_last_not_of(" \t\n\r");
            if (pv_start != std::string::npos) {
                prop_value = prop_value.substr(pv_start, pv_end - pv_start + 1);
            }

            if (!prop_name.empty() && !prop_value.empty()) {
                properties[prop_name] = prop_value;
            }

            if (prop_pos < props_block.size() && props_block[prop_pos] == ';') {
                prop_pos++;  // Skip ';'
            }
        }

        // Extract CSS variables from :root selector
        if (selector == ":root" && !properties.empty()) {
            for (const auto& [prop_name, prop_value] : properties) {
                // CSS variables start with --
                if (prop_name.length() >= 2 && prop_name.substr(0, 2) == "--") {
                    variable_resolver_.set_variable(prop_name, prop_value);
                }
            }
        }

        // Store the rule
        if (!selector.empty() && !properties.empty()) {
            CSSRule rule;
            rule.selector = selector;
            rule.properties = properties;
            rule.specificity = matcher_.calculate_specificity(selector);
            rules_.push_back(rule);
        }
    }

    return success;
}

void EnhancedStyleSheet::add_rule(
    const std::string& selector,
    const std::map<std::string, std::string>& properties) {

    // Store the rule for later application
    CSSRule rule;
    rule.selector = selector;
    rule.properties = properties;
    rule.specificity = matcher_.calculate_specificity(selector);
    rules_.push_back(rule);

    // Build CSS string from rule
    std::ostringstream css;
    css << selector << " { ";
    for (const auto& [key, value] : properties) {
        css << key << ": " << value << "; ";
    }
    css << "}";

    // Parse and add to stylesheet (for Lexbor's internal structures)
    parse_css(css.str());

    // Clear cache since rules changed
    clear_cache();
}

std::map<std::string, std::string> EnhancedStyleSheet::compute_style(
    const std::string& shape_id,
    const std::string& shape_type,
    const std::vector<std::string>& classes,
    const std::map<std::string, std::string>& attributes,
    const std::set<std::string>& pseudo_states,
    const std::map<std::string, std::string>& inline_style,
    const std::map<std::string, std::string>& parent_style,
    int child_index,
    int total_siblings) {

    // Generate cache key
    std::string cache_key = generate_cache_key(shape_id, shape_type, classes, pseudo_states, attributes, inline_style);

    // Check cache
    auto it = cache_.find(cache_key);
    if (it != cache_.end()) {
        cache_stats_.hits++;

        // Resolve variables in cached result and return
        auto result = it->second;
        for (auto& [key, value] : result) {
            value = variable_resolver_.resolve(value);
        }
        return result;
    }

    // Cache miss - compute style
    cache_stats_.misses++;

    // Pass rules to the computer
    auto result = computer_.compute_style(
        parser_.get_stylesheet(),
        rules_,  // Pass our stored rules!
        shape_id, shape_type, classes, attributes, pseudo_states,
        inline_style, parent_style
    );

    // Resolve CSS variables in the computed style
    for (auto& [key, value] : result) {
        value = variable_resolver_.resolve(value);
    }

    // Store the full result in the cache
    if (cache_.size() >= MAX_CACHE_SIZE) {
        evict_lru();
    }
    
    cache_[cache_key] = result;
    cache_stats_.size = cache_.size();

    return result;
}

void EnhancedStyleSheet::clear_cache() {
    cache_.clear();
    cache_stats_ = CacheStats();
    // DON'T clear rules_! They contain parsed CSS selectors/properties
    // rules_.clear();  // REMOVED - was causing CSS rules to be deleted on variable update!
}

std::string EnhancedStyleSheet::generate_cache_key(
    const std::string& shape_id,
    const std::string& shape_type,
    const std::vector<std::string>& classes,
    const std::set<std::string>& pseudo_states,
    const std::map<std::string, std::string>& attributes,
    const std::map<std::string, std::string>& inline_style) const {
    
    std::ostringstream key;
    key << shape_id << "|" << shape_type << "|";
    
    for (const auto& cls : classes) {
        key << cls << ",";
    }
    key << "|";
    
    for (const auto& state : pseudo_states) {
        key << state << ",";
    }

    key << "|";
    for (const auto& [attr_name, attr_value] : attributes) {
        key << attr_name << "=" << attr_value << ";";
    }
    
    key << "|";
    for (const auto& [style_name, style_value] : inline_style) {
        key << style_name << ":" << style_value << ";";
    }
    
    return key.str();
}

void EnhancedStyleSheet::evict_lru() {
    // Simple eviction: remove first entry
    // TODO: Implement proper LRU with access tracking
    if (!cache_.empty()) {
        cache_.erase(cache_.begin());
    }
}

// ============================================================================
// Calc() Expression Evaluation
// ============================================================================

std::optional<LexborCSSParser::CalcResult> 
LexborCSSParser::evaluate_calc(const std::string& expr, float context_value) const {
    // Remove "calc(" prefix and ")" suffix
    std::string trimmed = expr;
    
    // Trim whitespace
    size_t start = trimmed.find_first_not_of(" \t\n\r");
    size_t end = trimmed.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return std::nullopt;
    }
    trimmed = trimmed.substr(start, end - start + 1);
    
    // Check for calc() wrapper
    if (trimmed.substr(0, 5) == "calc(" && trimmed.back() == ')') {
        trimmed = trimmed.substr(5, trimmed.size() - 6);
    }
    
    // Tokenize the expression
    auto tokens = tokenize_calc(trimmed);
    if (tokens.empty()) {
        return std::nullopt;
    }
    
    // Evaluate the tokens
    return evaluate_tokens(tokens, context_value);
}

std::vector<LexborCSSParser::Token> 
LexborCSSParser::tokenize_calc(const std::string& expr) const {
    std::vector<Token> tokens;
    size_t i = 0;
    
    while (i < expr.size()) {
        char c = expr[i];
        
        // Skip whitespace
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            i++;
            continue;
        }
        
        // Operators
        if (c == '+' || c == '-' || c == '*' || c == '/') {
            Token token;
            token.type = Token::OPERATOR;
            token.op = c;
            tokens.push_back(token);
            i++;
            continue;
        }
        
        // Parentheses
        if (c == '(') {
            Token token;
            token.type = Token::LPAREN;
            tokens.push_back(token);
            i++;
            continue;
        }
        
        if (c == ')') {
            Token token;
            token.type = Token::RPAREN;
            tokens.push_back(token);
            i++;
            continue;
        }
        
        // Numbers
        if (std::isdigit(c) || c == '.') {
            size_t start = i;
            while (i < expr.size() && (std::isdigit(expr[i]) || expr[i] == '.')) {
                i++;
            }
            
            // Parse unit
            std::string unit;
            while (i < expr.size() && std::isalpha(expr[i])) {
                unit += expr[i];
                i++;
            }
            
            // Handle percentage
            if (i < expr.size() && expr[i] == '%') {
                unit = "%";
                i++;
            }
            
            Token token;
            token.type = Token::NUMBER;
            token.value = std::stof(expr.substr(start, i - start));
            token.unit = unit;
            tokens.push_back(token);
            continue;
        }
        
        // Unknown character
        return {};
    }
    
    // Add END token
    Token end_token;
    end_token.type = Token::END;
    tokens.push_back(end_token);
    
    return tokens;
}

std::optional<LexborCSSParser::CalcResult> 
LexborCSSParser::evaluate_tokens(const std::vector<Token>& tokens, float context_value) const {
    size_t pos = 0;
    return parse_expression(tokens, pos, context_value);
}

// Parse expression: term (('+' | '-') term)*
std::optional<LexborCSSParser::CalcResult> 
LexborCSSParser::parse_expression(const std::vector<Token>& tokens, size_t& pos, float context_value) const {
    auto left = parse_term(tokens, pos, context_value);
    if (!left) {
        return std::nullopt;
    }
    
    while (pos < tokens.size() && tokens[pos].type == Token::OPERATOR &&
           (tokens[pos].op == '+' || tokens[pos].op == '-')) {
        char op = tokens[pos].op;
        pos++;
        
        auto right = parse_term(tokens, pos, context_value);
        if (!right) {
            return std::nullopt;
        }
        
        // Check unit compatibility
        if (!left->unit.empty() && !right->unit.empty() && left->unit != right->unit) {
            return std::nullopt;  // Can't add/subtract different units
        }
        
        // Perform operation
        if (op == '+') {
            left->value += right->value;
        } else {
            left->value -= right->value;
        }
        
        // Keep the unit from the left operand
        if (left->unit.empty() && !right->unit.empty()) {
            left->unit = right->unit;
        }
    }
    
    return left;
}

// Parse term: factor (('*' | '/') factor)*
std::optional<LexborCSSParser::CalcResult> 
LexborCSSParser::parse_term(const std::vector<Token>& tokens, size_t& pos, float context_value) const {
    auto left = parse_factor(tokens, pos, context_value);
    if (!left) {
        return std::nullopt;
    }
    
    while (pos < tokens.size() && tokens[pos].type == Token::OPERATOR &&
           (tokens[pos].op == '*' || tokens[pos].op == '/')) {
        char op = tokens[pos].op;
        pos++;
        
        auto right = parse_factor(tokens, pos, context_value);
        if (!right) {
            return std::nullopt;
        }
        
        // Multiplication/division rules:
        // - At least one operand must be unitless
        // - Result takes unit from the other operand
        if (!left->unit.empty() && !right->unit.empty()) {
            return std::nullopt;  // Can't multiply/divide two values with units
        }
        
        if (op == '*') {
            left->value *= right->value;
            if (left->unit.empty() && !right->unit.empty()) {
                left->unit = right->unit;
            }
        } else {
            // Division by zero check
            if (right->value == 0.0f) {
                return std::nullopt;
            }
            left->value /= right->value;
            // For division, left keeps its unit
        }
    }
    
    return left;
}

// Parse factor: NUMBER | '(' expression ')'
std::optional<LexborCSSParser::CalcResult> 
LexborCSSParser::parse_factor(const std::vector<Token>& tokens, size_t& pos, float context_value) const {
    if (pos >= tokens.size()) {
        return std::nullopt;
    }
    
    const Token& token = tokens[pos];
    
    // Number
    if (token.type == Token::NUMBER) {
        pos++;
        CalcResult result;
        result.value = token.value;
        result.unit = token.unit;
        
        // Convert percentage to absolute value if context is provided
        if (result.unit == "%" && context_value != 0.0f) {
            result.value = (result.value / 100.0f) * context_value;
            result.unit = "px";  // Assume px after percentage conversion
        }
        
        return result;
    }
    
    // Parenthesized expression
    if (token.type == Token::LPAREN) {
        pos++;
        auto result = parse_expression(tokens, pos, context_value);
        if (!result) {
            return std::nullopt;
        }
        
        // Expect closing parenthesis
        if (pos >= tokens.size() || tokens[pos].type != Token::RPAREN) {
            return std::nullopt;
        }
        pos++;
        
        return result;
    }
    
    return std::nullopt;
}

// ============================================================================
// CSS Variable Resolver Implementation
// ============================================================================

void CSSVariableResolver::set_variable(const std::string& name, const std::string& value) {
    variables_[name] = value;
}

void CSSVariableResolver::remove_variable(const std::string& name) {
    variables_.erase(name);
}

bool CSSVariableResolver::has_variable(const std::string& name) const {
    if (variables_.count(name) > 0) {
        return true;
    }
    if (parent_) {
        return parent_->has_variable(name);
    }
    return false;
}

std::string CSSVariableResolver::get_variable(const std::string& name) const {
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        return it->second;
    }
    if (parent_) {
        return parent_->get_variable(name);
    }
    return "";
}

std::string CSSVariableResolver::resolve(const std::string& value) const {
    std::string result = value;
    size_t pos = 0;
    
    // Find and resolve all var() expressions
    while ((pos = result.find("var(", pos)) != std::string::npos) {
        // Find the matching closing parenthesis
        size_t start = pos;
        size_t paren_count = 1;
        size_t i = pos + 4;  // Skip "var("
        
        while (i < result.size() && paren_count > 0) {
            if (result[i] == '(') {
                paren_count++;
            } else if (result[i] == ')') {
                paren_count--;
            }
            i++;
        }
        
        if (paren_count != 0) {
            // Unmatched parentheses - skip this var()
            pos++;
            continue;
        }
        
        // Extract the var() expression
        std::string var_expr = result.substr(start, i - start);
        
        // Resolve it
        std::string resolved = resolve_var(var_expr);
        
        // Replace in result
        result.replace(start, i - start, resolved);
        
        // Continue from after the replacement
        pos = start + resolved.size();
    }
    
    return result;
}

CSSVariableResolver CSSVariableResolver::create_scope() const {
    return CSSVariableResolver(this);
}

void CSSVariableResolver::clear() {
    variables_.clear();
}

std::string CSSVariableResolver::resolve_var(const std::string& expr) const {
    std::string var_name;
    std::string fallback;
    
    if (!parse_var_expression(expr, var_name, fallback)) {
        // Invalid syntax - return as-is
        return expr;
    }
    
    // Check if variable exists
    if (has_variable(var_name)) {
        return get_variable(var_name);
    }
    
    // Variable doesn't exist - use fallback
    if (!fallback.empty()) {
        // Recursively resolve fallback (it might contain var() too)
        return resolve(fallback);
    }
    
    // No fallback - return empty or the original expression
    return expr;
}

bool CSSVariableResolver::parse_var_expression(
    const std::string& expr, 
    std::string& var_name, 
    std::string& fallback) const {
    
    // Check if it starts with "var("
    if (expr.substr(0, 4) != "var(") {
        return false;
    }
    
    // Check if it ends with ")"
    if (expr.back() != ')') {
        return false;
    }
    
    // Extract content between "var(" and ")"
    std::string content = expr.substr(4, expr.size() - 5);
    
    // Trim whitespace
    size_t start = content.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return false;  // Empty content
    }
    content = content.substr(start);
    
    // Find comma separator (if any)
    size_t comma_pos = std::string::npos;
    int paren_depth = 0;
    
    for (size_t i = 0; i < content.size(); i++) {
        if (content[i] == '(') {
            paren_depth++;
        } else if (content[i] == ')') {
            paren_depth--;
        } else if (content[i] == ',' && paren_depth == 0) {
            comma_pos = i;
            break;
        }
    }
    
    if (comma_pos != std::string::npos) {
        // Has fallback
        var_name = content.substr(0, comma_pos);
        fallback = content.substr(comma_pos + 1);
        
        // Trim both
        var_name.erase(var_name.find_last_not_of(" \t\n\r") + 1);
        fallback.erase(0, fallback.find_first_not_of(" \t\n\r"));
        fallback.erase(fallback.find_last_not_of(" \t\n\r") + 1);
    } else {
        // No fallback
        var_name = content;
        var_name.erase(var_name.find_last_not_of(" \t\n\r") + 1);
        fallback = "";
    }
    
    // Validate variable name (must start with --)
    if (var_name.size() < 2 || var_name.substr(0, 2) != "--") {
        return false;
    }
    
    return true;
}

 
// ============================================================================
// EnhancedStyleSheet - Keyframe Animation Support
// ============================================================================

void EnhancedStyleSheet::add_keyframe_animation(const KeyframeAnimation& animation) {
    keyframe_animations_[animation.name] = animation;
}

const KeyframeAnimation* EnhancedStyleSheet::get_keyframe_animation(const std::string& name) const {
    auto it = keyframe_animations_.find(name);
    return it != keyframe_animations_.end() ? &it->second : nullptr;
}

// ============================================================================
// NEW: Typed Style Computation (60fps Refactor)
// ============================================================================

nvgcss::ComputedStyle EnhancedStyleSheet::compute_style_typed(
    const std::string& shape_id,
    const std::string& shape_type,
    const std::vector<std::string>& classes,
    const std::map<std::string, std::string>& attributes,
    const std::set<std::string>& pseudo_states,
    const std::map<std::string, std::string>& inline_style,
    const nvgcss::ComputedStyle* parent_style,
    int child_index,
    int total_siblings) {

    // Step 1: Get string-based style using existing implementation
    std::map<std::string, std::string> parent_style_map;
    if (parent_style) {
        // Inheritance handled via string-based system
    }

    auto style_map = compute_style(
        shape_id, shape_type, classes, attributes, pseudo_states,
        inline_style, parent_style_map, child_index, total_siblings
    );

    // Step 2: Convert string map to typed ComputedStyle
    nvgcss::ComputedStyle result;

    // Helper lambda to get value with default
    auto get = [&style_map, &inline_style](const std::string& key) -> std::string {
        // CSS cascade: inline styles override stylesheet styles
        auto inline_it = inline_style.find(key);
        if (inline_it != inline_style.end()) {
            return inline_it->second;
        }
        auto it = style_map.find(key);
        return it != style_map.end() ? it->second : "";
    };

    // === Display & Positioning ===
    result.display = nvgcss::convert::parse_display(get("display"));
    result.position = nvgcss::convert::parse_position(get("position"));
    result.box_sizing = nvgcss::convert::parse_box_sizing(get("box-sizing"));

    // === Dimensions ===
    if (auto w = nvgcss::convert::parse_length(get("width"))) result.width = *w;
    if (auto h = nvgcss::convert::parse_length(get("height"))) result.height = *h;
    if (auto mw = nvgcss::convert::parse_length(get("min-width"))) result.min_width = *mw;
    if (auto mh = nvgcss::convert::parse_length(get("min-height"))) result.min_height = *mh;
    if (auto mxw = nvgcss::convert::parse_length(get("max-width"))) result.max_width = *mxw;
    if (auto mxh = nvgcss::convert::parse_length(get("max-height"))) result.max_height = *mxh;

    // === Position Offsets ===
    if (auto t = nvgcss::convert::parse_length(get("top"))) result.top = *t;
    if (auto r = nvgcss::convert::parse_length(get("right"))) result.right = *r;
    if (auto b = nvgcss::convert::parse_length(get("bottom"))) result.bottom = *b;
    if (auto l = nvgcss::convert::parse_length(get("left"))) result.left = *l;

    // Parse z-index
    std::string z = get("z-index");
    if (!z.empty() && z != "auto") {
        result.z_index = std::atoi(z.c_str());
    }

    // === Box Model ===
    // Padding (shorthand or individual sides)
    std::string padding = get("padding");
    if (!padding.empty()) {
        result.padding = nvgcss::convert::parse_box_sides(padding);
    } else {
        // Individual sides
        if (auto pt = nvgcss::convert::parse_length(get("padding-top"))) result.padding[0] = *pt;
        if (auto pr = nvgcss::convert::parse_length(get("padding-right"))) result.padding[1] = *pr;
        if (auto pb = nvgcss::convert::parse_length(get("padding-bottom"))) result.padding[2] = *pb;
        if (auto pl = nvgcss::convert::parse_length(get("padding-left"))) result.padding[3] = *pl;
    }

    // Margin (shorthand or individual sides)
    std::string margin = get("margin");
    if (!margin.empty()) {
        result.margin = nvgcss::convert::parse_box_sides(margin);
    } else {
        if (auto mt = nvgcss::convert::parse_length(get("margin-top"))) result.margin[0] = *mt;
        if (auto mr = nvgcss::convert::parse_length(get("margin-right"))) result.margin[1] = *mr;
        if (auto mb = nvgcss::convert::parse_length(get("margin-bottom"))) result.margin[2] = *mb;
        if (auto ml = nvgcss::convert::parse_length(get("margin-left"))) result.margin[3] = *ml;
    }

    // Border
    // Border width
    std::string border_width_str = get("border-width");
    if (!border_width_str.empty()) {
        auto widths = nvgcss::convert::parse_box_sides(border_width_str);
        for (int i = 0; i < 4; ++i) {
            result.border.width[i] = widths[i].resolve(0, 16, 800);  // Convert to pixels
        }
    } else {
        if (auto btw = nvgcss::convert::parse_length(get("border-top-width")))
            result.border.width[0] = btw->resolve(0, 16, 800);
        if (auto brw = nvgcss::convert::parse_length(get("border-right-width")))
            result.border.width[1] = brw->resolve(0, 16, 800);
        if (auto bbw = nvgcss::convert::parse_length(get("border-bottom-width")))
            result.border.width[2] = bbw->resolve(0, 16, 800);
        if (auto blw = nvgcss::convert::parse_length(get("border-left-width")))
            result.border.width[3] = blw->resolve(0, 16, 800);
    }

    // Border style
    result.border.style[0] = nvgcss::convert::parse_border_style(get("border-top-style"));
    result.border.style[1] = nvgcss::convert::parse_border_style(get("border-right-style"));
    result.border.style[2] = nvgcss::convert::parse_border_style(get("border-bottom-style"));
    result.border.style[3] = nvgcss::convert::parse_border_style(get("border-left-style"));

    // Border color
    if (auto btc = nvgcss::convert::parse_color(get("border-top-color")))
        result.border.color[0] = *btc;
    if (auto brc = nvgcss::convert::parse_color(get("border-right-color")))
        result.border.color[1] = *brc;
    if (auto bbc = nvgcss::convert::parse_color(get("border-bottom-color")))
        result.border.color[2] = *bbc;
    if (auto blc = nvgcss::convert::parse_color(get("border-left-color")))
        result.border.color[3] = *blc;

    // Border radius
    std::string border_radius_str = get("border-radius");
    if (!border_radius_str.empty()) {
        auto radii = nvgcss::convert::parse_box_sides(border_radius_str);
        for (int i = 0; i < 4; ++i) {
            // Don't resolve percentages here - mark with negative value
            // Layout engine will resolve based on element dimensions
            if (radii[i].unit == nvgcss::LengthUnit::PERCENT) {
                result.border.radius[i] = -radii[i].value;  // Store as negative percentage
            } else {
                result.border.radius[i] = radii[i].resolve(0, 16, 800);
            }
        }
    }

    // === Overflow ===
    result.overflow_x = nvgcss::convert::parse_overflow(get("overflow-x"));
    result.overflow_y = nvgcss::convert::parse_overflow(get("overflow-y"));
    std::string overflow = get("overflow");
    if (!overflow.empty()) {
        auto o = nvgcss::convert::parse_overflow(overflow);
        result.overflow_x = result.overflow_y = o;
    }

    // === Background ===
    std::string bg_str = get("background");
    std::string bg_color_str = get("background-color");
    
    // Check background first (can be gradient or color)
    if (!bg_str.empty()) {
        // Check if it's a gradient
        if (bg_str.find("gradient") != std::string::npos) {
            // Store gradient string - painter will parse it
            result.background.type = nvgcss::BackgroundType::GRADIENT;
            result.background.gradient_css = bg_str;
        }
        // Try to parse as color
        else if (auto c = nvgcss::convert::parse_color(bg_str)) {
            result.background = nvgcss::Background::solid(*c);
        }
    }
    // Then check background-color (overrides if both present)
    else if (!bg_color_str.empty()) {
        if (auto c = nvgcss::convert::parse_color(bg_color_str)) {
            result.background = nvgcss::Background::solid(*c);
        }
    }

    // Opacity
    std::string opacity_str = get("opacity");
    if (!opacity_str.empty()) {
        result.opacity = std::strtof(opacity_str.c_str(), nullptr);
    }

    // === Flexbox ===
    // Only parse if value is non-empty to preserve typed struct defaults
    std::string flex_direction_str = get("flex-direction");
    if (!flex_direction_str.empty()) result.flex_direction = nvgcss::convert::parse_flex_direction(flex_direction_str);

    std::string flex_wrap_str = get("flex-wrap");
    if (!flex_wrap_str.empty()) result.flex_wrap = nvgcss::convert::parse_flex_wrap(flex_wrap_str);

    std::string justify_content_str = get("justify-content");
    if (!justify_content_str.empty()) result.justify_content = nvgcss::convert::parse_justify_content(justify_content_str);

    std::string align_items_str = get("align-items");
    if (!align_items_str.empty()) result.align_items = nvgcss::convert::parse_align_items(align_items_str);

    std::string align_content_str = get("align-content");
    if (!align_content_str.empty()) result.align_content = nvgcss::convert::parse_align_content(align_content_str);

    std::string flex_grow_str = get("flex-grow");
    if (!flex_grow_str.empty()) result.flex_grow = std::strtof(flex_grow_str.c_str(), nullptr);

    std::string flex_shrink_str = get("flex-shrink");
    if (!flex_shrink_str.empty()) result.flex_shrink = std::strtof(flex_shrink_str.c_str(), nullptr);

    if (auto fb = nvgcss::convert::parse_length(get("flex-basis"))) result.flex_basis = *fb;

    std::string order_str = get("order");
    if (!order_str.empty()) result.order = std::atoi(order_str.c_str());

    if (auto g = nvgcss::convert::parse_length(get("gap"))) result.gap = *g;

    // === Text ===
    if (auto c = nvgcss::convert::parse_color(get("color"))) result.color = *c;

    std::string font_size_str = get("font-size");
    if (!font_size_str.empty()) {
        if (auto fs = nvgcss::convert::parse_length(font_size_str)) {
            result.font_size = fs->resolve(16.0f, 16.0f, 800.0f);  // Resolve to pixels
        }
    }

    result.font_weight = nvgcss::convert::parse_font_weight(get("font-weight"));
    result.font_style = nvgcss::convert::parse_font_style(get("font-style"));
    result.text_align = nvgcss::convert::parse_text_align(get("text-align"));

    std::string font_family_str = get("font-family");
    if (!font_family_str.empty()) {
        result.font_family = font_family_str;
    }

    // === Grid Properties ===
    std::string grid_template_rows_str = get("grid-template-rows");
    if (!grid_template_rows_str.empty()) {
        result.grid_template_rows = nvgcss::convert::parse_grid_track_list(grid_template_rows_str);
    }

    std::string grid_template_columns_str = get("grid-template-columns");
    if (!grid_template_columns_str.empty()) {
        result.grid_template_columns = nvgcss::convert::parse_grid_track_list(grid_template_columns_str);
    }

    if (auto row_gap = nvgcss::convert::parse_length(get("grid-row-gap"))) {
        result.grid_row_gap = *row_gap;
    } else if (auto row_gap2 = nvgcss::convert::parse_length(get("row-gap"))) {
        result.grid_row_gap = *row_gap2;
    }

    if (auto col_gap = nvgcss::convert::parse_length(get("grid-column-gap"))) {
        result.grid_column_gap = *col_gap;
    } else if (auto col_gap2 = nvgcss::convert::parse_length(get("column-gap"))) {
        result.grid_column_gap = *col_gap2;
    }

    // Additional properties (box-shadows, text-shadows, transforms) are handled
    // by the string-based system and converted during rendering

    // === SVG Stroke Properties (Rough Rendering) ===
    std::string stroke_rendering_str = get("stroke-rendering");
    if (!stroke_rendering_str.empty()) {
        if (stroke_rendering_str == "rough") {
            result.svg_stroke.rendering = nvgcss::StrokeRendering::ROUGH;
        } else {
            result.svg_stroke.rendering = nvgcss::StrokeRendering::AUTO;
        }
    }

    std::string roughness_str = get("roughness");
    if (!roughness_str.empty()) {
        result.svg_stroke.roughness = std::strtof(roughness_str.c_str(), nullptr);
    }

    std::string bowing_str = get("bowing");
    if (!bowing_str.empty()) {
        result.svg_stroke.bowing = std::strtof(bowing_str.c_str(), nullptr);
    }

    std::string stroke_count_str = get("stroke-count");
    if (!stroke_count_str.empty()) {
        result.svg_stroke.stroke_count = std::atoi(stroke_count_str.c_str());
    }

    std::string seed_str = get("seed");
    if (!seed_str.empty()) {
        result.svg_stroke.seed = std::atoi(seed_str.c_str());
    }

    return result;
}

} // namespace lexbor
} // namespace nanovg_css

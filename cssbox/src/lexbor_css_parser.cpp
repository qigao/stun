#include "lexbor_css_parser.h"
#include "cssbox_internal.h"
#include "cssbox_types.h"
#include "cssbox_conversion.h"
#include "cssbox_defaults.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <memory>
#include <fmtlog.h>

namespace cssbox {
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
            std::string class_name;

            // Parse class name with support for CSS escapes (\:, \/, etc.)
            while (i < trimmed.size()) {
                char ch = trimmed[i];

                // Handle CSS escape sequences
                if (ch == '\\' && i + 1 < trimmed.size()) {
                    // Skip backslash and add the escaped character
                    i++;
                    class_name += trimmed[i];
                    i++;
                }
                // Valid class name characters (NOTE: ':' is NOT included here, it's a pseudo-class separator)
                else if (std::isalnum(ch) || ch == '_' || ch == '-' || ch == '/') {
                    class_name += ch;
                    i++;
                }
                else {
                    break;  // End of class name (could be ':', '[', etc.)
                }
            }

            if (!class_name.empty()) {
                result.classes.push_back(class_name);
            }
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

    // DEBUG: Log element classes for page elements and content
    bool debug_this = shape_id.find("page-") != std::string::npos ||
                      shape_id == "content" || shape_id == "root" || shape_id == "header";
    // Also debug elements with stat-* classes
    for (const auto& cls : classes) {
        if (cls.find("stat-") == 0 || cls == "stats-row") {
            debug_this = true;
            break;
        }
    }
    if (debug_this) {
        std::string class_list;
        for (const auto& cls : classes) {
            class_list += cls + ",";
        }
        logi("[STYLE] compute_style id='{}' type='{}' classes=[{}] rules_count={}",
             shape_id, shape_type, class_list, rules.size());
    }

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
            // DEBUG: Log matching rules
            if (debug_this) {
                logi("[STYLE]   MATCH: selector='{}' spec={}", rule.selector, rule.specificity);
            }
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

    // DEBUG: Log final computed style for key elements
    if (debug_this) {
        auto display_it = result.find("display");
        auto height_it = result.find("height");
        std::string display_val = (display_it != result.end()) ? display_it->second : "(not set)";
        std::string height_val = (height_it != result.end()) ? height_it->second : "(auto)";
        logi("[STYLE]   RESULT: id='{}' display='{}' height='{}'", shape_id, display_val, height_val);
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
    using namespace cssbox::defaults;
    
    std::map<std::string, std::string> defaults;

    // Common CSS defaults (W3C standards)
    defaults["background"] = BACKGROUND;
    defaults["color"] = COLOR;
    defaults["opacity"] = OPACITY;

    // SVG defaults (SVG 2.0 standard)
    defaults["fill"] = FILL;
    defaults["stroke"] = STROKE;
    defaults["stroke-width"] = STROKE_WIDTH;

    // Type-specific defaults
    if (shape_type == "text") {
        defaults["font-family"] = FONT_FAMILY;
        defaults["font-size"] = FONT_SIZE;      // CSS standard: 16px
        defaults["font-weight"] = FONT_WEIGHT;
        defaults["text-align"] = TEXT_ALIGN;
        defaults["color"] = COLOR;
        defaults["fill"] = COLOR;               // Text fill matches color
        defaults["background"] = BACKGROUND;
    } else if (shape_type == "rect" || shape_type == "circle" ||
               shape_type == "ellipse" || shape_type == "path") {
        // SVG shapes: SVG 2.0 standard (no default fill)
        defaults["fill"] = FILL;                // SVG standard: none
        defaults["stroke"] = STROKE;            // SVG standard: none
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
// Helper: Expand animation shorthand property
// ============================================================================

/**
 * @brief Expand animation shorthand into individual properties
 *
 * Format: animation: [name] [duration] [timing-function] [delay] [iteration-count] [direction] [fill-mode];
 * Example: "spin 1s linear infinite" ->
 *   - animation-name: spin
 *   - animation-duration: 1s
 *   - animation-timing-function: linear
 *   - animation-iteration-count: infinite
 */
static void expand_animation_shorthand(const std::string& value,
                                       std::map<std::string, std::string>& properties) {
    std::istringstream iss(value);
    std::vector<std::string> tokens;
    std::string token;

    // Split by whitespace
    while (iss >> token) {
        tokens.push_back(token);
    }

    if (tokens.empty()) return;

    // Animation keywords for identification
    std::set<std::string> timing_functions = {
        "linear", "ease", "ease-in", "ease-out", "ease-in-out", "step-start", "step-end"
    };
    std::set<std::string> directions = {
        "normal", "reverse", "alternate", "alternate-reverse"
    };
    std::set<std::string> fill_modes = {
        "none", "forwards", "backwards", "both"
    };
    std::set<std::string> play_states = {
        "running", "paused"
    };

    // Parse tokens
    std::string name;
    std::string duration;
    std::string timing_function;
    std::string delay;
    std::string iteration_count;
    std::string direction;
    std::string fill_mode;

    for (const auto& tok : tokens) {
        // Check if it's a time value (duration or delay)
        // Time values must start with a digit: "1s", "0.5s", "100ms"
        // This prevents "spin", "pulse" from being treated as time values
        if (!tok.empty() && std::isdigit(tok[0]) && tok.find('s') != std::string::npos) {
            if (duration.empty()) {
                duration = tok;
            } else if (delay.empty()) {
                delay = tok;
            }
        }
        // Check if it's a timing function
        else if (timing_functions.count(tok) || tok.find("cubic-bezier") != std::string::npos) {
            timing_function = tok;
        }
        // Check if it's iteration count
        else if (tok == "infinite" || std::isdigit(tok[0])) {
            iteration_count = tok;
        }
        // Check if it's direction
        else if (directions.count(tok)) {
            direction = tok;
        }
        // Check if it's fill mode
        else if (fill_modes.count(tok)) {
            fill_mode = tok;
        }
        // Otherwise, it's the animation name (first non-keyword token)
        else if (name.empty()) {
            name = tok;
        }
    }

    // Set properties with defaults
    if (!name.empty()) {
        properties["animation-name"] = name;
    }
    if (!duration.empty()) {
        properties["animation-duration"] = duration;
    }
    if (!timing_function.empty()) {
        properties["animation-timing-function"] = timing_function;
    }
    if (!delay.empty()) {
        properties["animation-delay"] = delay;
    }
    if (!iteration_count.empty()) {
        properties["animation-iteration-count"] = iteration_count;
    }
    if (!direction.empty()) {
        properties["animation-direction"] = direction;
    }
    if (!fill_mode.empty()) {
        properties["animation-fill-mode"] = fill_mode;
    }
}

// ============================================================================
// EnhancedStyleSheet Implementation
// ============================================================================

bool EnhancedStyleSheet::parse_css(const std::string& css) {
    clear_cache();

    // DEBUG: Log CSS content to verify @keyframes are present
    logi("[CSS PARSE] Starting parse_css, CSS length: {}", css.length());
    if (css.find("@keyframes") != std::string::npos) {
        logi("[CSS PARSE] ✓ CSS contains '@keyframes' keyword");
        size_t kf_pos = css.find("@keyframes");
        logi("[CSS PARSE]   First @keyframes at position {}", kf_pos);
        logi("[CSS PARSE]   Context: '{}'", css.substr(kf_pos, std::min(size_t(80), css.length() - kf_pos)));
    } else {
        logw("[CSS PARSE] ✗ CSS does NOT contain '@keyframes' keyword");
    }

    // Parse with Lexbor (for validation and internal structures)
    bool success = parser_.parse(css);

    // Extract rules manually (since Lexbor doesn't expose them)
    // Simple CSS parser to extract selector { prop: value; ... } blocks
    size_t pos = 0;
    int loop_iteration = 0;
    while (pos < css.size()) {
        loop_iteration++;

        // Skip whitespace and comments
        while (pos < css.size() && (css[pos] == ' ' || css[pos] == '\t' || css[pos] == '\n' || css[pos] == '\r')) {
            pos++;
        }

        if (pos >= css.size()) break;

        // DEBUG: Show what we're looking at (every 10th iteration to reduce spam)
        if (loop_iteration % 10 == 0 || (pos + 15 < css.size() && css.substr(pos, 10) == "@keyframes")) {
            std::string preview = css.substr(pos, std::min(size_t(40), css.size() - pos));
            // Replace newlines with spaces for cleaner log
            std::replace(preview.begin(), preview.end(), '\n', ' ');
            logi("[CSS PARSE] Loop #{}, pos={}, next: '{}'", loop_iteration, pos, preview);
        }

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

        // Check for @media rules
        if (css.substr(pos, 6) == "@media") {
            pos += 6;  // Skip "@media"

            // Skip whitespace
            while (pos < css.size() && (css[pos] == ' ' || css[pos] == '\t' || css[pos] == '\n')) {
                pos++;
            }

            // Extract media query (everything before '{')
            size_t query_start = pos;
            while (pos < css.size() && css[pos] != '{') {
                pos++;
            }

            if (pos >= css.size()) break;

            std::string media_query = css.substr(query_start, pos - query_start);
            // Trim
            size_t mq_start = media_query.find_first_not_of(" \t\n\r");
            size_t mq_end = media_query.find_last_not_of(" \t\n\r");
            if (mq_start != std::string::npos) {
                media_query = media_query.substr(mq_start, mq_end - mq_start + 1);
            }

            pos++;  // Skip '{'

            // Find matching '}' for @media block
            size_t media_block_start = pos;
            int brace_count = 1;
            while (pos < css.size() && brace_count > 0) {
                if (css[pos] == '{') brace_count++;
                else if (css[pos] == '}') brace_count--;
                if (brace_count > 0) pos++;
            }

            std::string media_block = css.substr(media_block_start, pos - media_block_start);
            pos++;  // Skip '}'

            // Recursively parse rules inside @media block with media_query set
            size_t media_pos = 0;
            while (media_pos < media_block.size()) {
                // Skip whitespace
                while (media_pos < media_block.size() && (media_block[media_pos] == ' ' || media_block[media_pos] == '\t' || media_block[media_pos] == '\n' || media_block[media_pos] == '\r')) {
                    media_pos++;
                }

                if (media_pos >= media_block.size()) break;

                // Skip comments
                if (media_pos + 1 < media_block.size() && media_block[media_pos] == '/' && media_block[media_pos + 1] == '*') {
                    media_pos += 2;
                    while (media_pos + 1 < media_block.size()) {
                        if (media_block[media_pos] == '*' && media_block[media_pos + 1] == '/') {
                            media_pos += 2;
                            break;
                        }
                        media_pos++;
                    }
                    continue;
                }

                // Find selector
                size_t inner_selector_start = media_pos;
                while (media_pos < media_block.size() && media_block[media_pos] != '{') {
                    media_pos++;
                }

                if (media_pos >= media_block.size()) break;

                std::string inner_selector = media_block.substr(inner_selector_start, media_pos - inner_selector_start);
                // Trim
                size_t is_start = inner_selector.find_first_not_of(" \t\n\r");
                size_t is_end = inner_selector.find_last_not_of(" \t\n\r");
                if (is_start != std::string::npos) {
                    inner_selector = inner_selector.substr(is_start, is_end - is_start + 1);
                }

                media_pos++;  // Skip '{'

                // Find properties block
                size_t inner_props_start = media_pos;
                int inner_brace_count = 1;
                while (media_pos < media_block.size() && inner_brace_count > 0) {
                    if (media_block[media_pos] == '{') inner_brace_count++;
                    else if (media_block[media_pos] == '}') inner_brace_count--;
                    if (inner_brace_count > 0) media_pos++;
                }

                std::string inner_props_block = media_block.substr(inner_props_start, media_pos - inner_props_start);
                media_pos++;  // Skip '}'

                // Parse properties (simplified - reuse existing logic below)
                std::map<std::string, std::string> inner_properties;
                size_t prop_pos = 0;
                while (prop_pos < inner_props_block.size()) {
                    // Skip whitespace
                    while (prop_pos < inner_props_block.size() && (inner_props_block[prop_pos] == ' ' || inner_props_block[prop_pos] == '\t' || inner_props_block[prop_pos] == '\n' || inner_props_block[prop_pos] == '\r')) {
                        prop_pos++;
                    }

                    if (prop_pos >= inner_props_block.size()) break;

                    // Find property name
                    size_t prop_name_start = prop_pos;
                    while (prop_pos < inner_props_block.size() && inner_props_block[prop_pos] != ':') {
                        prop_pos++;
                    }

                    if (prop_pos >= inner_props_block.size()) break;

                    std::string prop_name = inner_props_block.substr(prop_name_start, prop_pos - prop_name_start);
                    size_t pn_start = prop_name.find_first_not_of(" \t\n\r");
                    size_t pn_end = prop_name.find_last_not_of(" \t\n\r");
                    if (pn_start != std::string::npos) {
                        prop_name = prop_name.substr(pn_start, pn_end - pn_start + 1);
                    }
                    std::transform(prop_name.begin(), prop_name.end(), prop_name.begin(),
                                   [](unsigned char c){ return std::tolower(c); });

                    prop_pos++;  // Skip ':'

                    // Find property value
                    size_t prop_value_start = prop_pos;
                    while (prop_pos < inner_props_block.size() && inner_props_block[prop_pos] != ';' && inner_props_block[prop_pos] != '}') {
                        prop_pos++;
                    }

                    std::string prop_value = inner_props_block.substr(prop_value_start, prop_pos - prop_value_start);
                    size_t pv_start = prop_value.find_first_not_of(" \t\n\r");
                    size_t pv_end = prop_value.find_last_not_of(" \t\n\r");
                    if (pv_start != std::string::npos) {
                        prop_value = prop_value.substr(pv_start, pv_end - pv_start + 1);
                    }

                    if (!prop_name.empty() && !prop_value.empty()) {
                        inner_properties[prop_name] = prop_value;
                    }

                    if (prop_pos < inner_props_block.size() && inner_props_block[prop_pos] == ';') {
                        prop_pos++;
                    }
                }

                // Store rule with media_query - handle comma-separated selectors
                if (!inner_selector.empty() && !inner_properties.empty()) {
                    // Split selector on commas
                    std::vector<std::string> inner_individual_selectors;
                    size_t isel_pos = 0;
                    while (isel_pos < inner_selector.size()) {
                        size_t icomma_pos = inner_selector.find(',', isel_pos);
                        std::string isingle_sel;
                        if (icomma_pos != std::string::npos) {
                            isingle_sel = inner_selector.substr(isel_pos, icomma_pos - isel_pos);
                            isel_pos = icomma_pos + 1;
                        } else {
                            isingle_sel = inner_selector.substr(isel_pos);
                            isel_pos = inner_selector.size();
                        }
                        size_t is_start = isingle_sel.find_first_not_of(" \t\n\r");
                        size_t is_end = isingle_sel.find_last_not_of(" \t\n\r");
                        if (is_start != std::string::npos) {
                            isingle_sel = isingle_sel.substr(is_start, is_end - is_start + 1);
                        }
                        if (!isingle_sel.empty()) {
                            inner_individual_selectors.push_back(isingle_sel);
                        }
                    }

                    for (const auto& isingle_sel : inner_individual_selectors) {
                        CSSRule rule;
                        rule.selector = isingle_sel;
                        rule.properties = inner_properties;
                        rule.specificity = matcher_.calculate_specificity(isingle_sel);
                        rule.media_query = media_query;
                        rules_.push_back(rule);
                    }
                }
            }

            continue;  // Continue to next rule
        }

        // DEBUG: Check what we're seeing before @keyframes check
        if (pos + 15 < css.size()) {
            std::string next_10 = css.substr(pos, 10);
            if (next_10[0] == '@') {
                logi("[CSS PARSE] Found @ at pos={}, next 15 chars: '{}'", pos, css.substr(pos, 15));
                if (next_10 == "@keyframes") {
                    logi("[CSS PARSE] ✓ Matched @keyframes!");
                } else {
                    logi("[CSS PARSE] ✗ @ found but not @keyframes, got: '{}'", next_10);
                }
            }
        }

        // Check for @keyframes rules
        if (css.substr(pos, 10) == "@keyframes") {
            logi("[CSS PARSE] >>> Entering @keyframes parsing block");
            pos += 10;  // Skip "@keyframes"

            // Skip whitespace
            while (pos < css.size() && (css[pos] == ' ' || css[pos] == '\t' || css[pos] == '\n')) {
                pos++;
            }

            // Extract animation name (everything before '{')
            size_t name_start = pos;
            while (pos < css.size() && css[pos] != '{') {
                pos++;
            }

            if (pos >= css.size()) break;

            std::string anim_name = css.substr(name_start, pos - name_start);
            // Trim
            size_t an_start = anim_name.find_first_not_of(" \t\n\r");
            size_t an_end = anim_name.find_last_not_of(" \t\n\r");
            if (an_start != std::string::npos) {
                anim_name = anim_name.substr(an_start, an_end - an_start + 1);
            }

            pos++;  // Skip '{'

            // Find matching '}' for @keyframes block
            size_t keyframes_block_start = pos;
            int brace_count = 1;
            while (pos < css.size() && brace_count > 0) {
                if (css[pos] == '{') brace_count++;
                else if (css[pos] == '}') brace_count--;
                if (brace_count > 0) pos++;
            }

            std::string keyframes_block = css.substr(keyframes_block_start, pos - keyframes_block_start);
            pos++;  // Skip '}'

            // Parse and add keyframe animation
            if (!anim_name.empty()) {
                logi("[CSS PARSE] Found @keyframes rule: name='{}' block_size={}", anim_name, keyframes_block.size());
                ::KeyframeAnimation kf_anim = ::parse_keyframes_rule(keyframes_block, anim_name);
                add_keyframe_animation(kf_anim);
                logi("[CSS PARSE] Added keyframe animation '{}' with {} keyframes", anim_name, kf_anim.keyframes.size());
            }

            continue;  // Continue to next rule
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

            // Skip comments /* ... */ inside properties block (FIX: was missing)
            if (prop_pos + 1 < props_block.size() && props_block[prop_pos] == '/' && props_block[prop_pos + 1] == '*') {
                prop_pos += 2;
                while (prop_pos + 1 < props_block.size()) {
                    if (props_block[prop_pos] == '*' && props_block[prop_pos + 1] == '/') {
                        prop_pos += 2;
                        break;
                    }
                    prop_pos++;
                }
                continue;  // Skip to next iteration after comment
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
                // Expand animation shorthand property
                if (prop_name == "animation") {
                    expand_animation_shorthand(prop_value, properties);
                } else {
                    properties[prop_name] = prop_value;
                }
            }

            if (prop_pos < props_block.size() && props_block[prop_pos] == ';') {
                prop_pos++;  // Skip ';'
            }
        }

        // Extract CSS variables from :root selector
        if (selector == ":root") {
            // logi("[CSS PARSE] Found :root rule with {} properties", properties.size());
            if (!properties.empty()) {
                for (const auto& [prop_name, prop_value] : properties) {
                    // logi("[CSS PARSE]   Property: '{}' = '{}'", prop_name, prop_value);
                    // CSS variables start with --
                    if (prop_name.length() >= 2 && prop_name.substr(0, 2) == "--") {
                        // logi("[CSS PARSE]   Setting variable: '{}' = '{}'", prop_name, prop_value);
                        variable_resolver_.set_variable(prop_name, prop_value);
                    }
                }
            }
            // Don't store the :root rule itself, just the variables
            continue;
        }

        // Store the rule - handle comma-separated selectors
        if (!selector.empty() && !properties.empty()) {
            // Split selector on commas to handle grouped selectors like "#a, #b, #c { ... }"
            std::vector<std::string> individual_selectors;
            size_t sel_pos = 0;
            while (sel_pos < selector.size()) {
                size_t comma_pos = selector.find(',', sel_pos);
                std::string single_sel;
                if (comma_pos != std::string::npos) {
                    single_sel = selector.substr(sel_pos, comma_pos - sel_pos);
                    sel_pos = comma_pos + 1;
                } else {
                    single_sel = selector.substr(sel_pos);
                    sel_pos = selector.size();
                }
                // Trim whitespace from individual selector
                size_t s_start = single_sel.find_first_not_of(" \t\n\r");
                size_t s_end = single_sel.find_last_not_of(" \t\n\r");
                if (s_start != std::string::npos) {
                    single_sel = single_sel.substr(s_start, s_end - s_start + 1);
                }
                if (!single_sel.empty()) {
                    individual_selectors.push_back(single_sel);
                }
            }

            // Create a separate rule for each selector
            for (const auto& single_sel : individual_selectors) {
                CSSRule rule;
                rule.selector = single_sel;
                rule.properties = properties;
                rule.specificity = matcher_.calculate_specificity(single_sel);
                rules_.push_back(rule);
            }
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

    // Filter rules by media query
    std::vector<CSSRule> filtered_rules;
    // logi("[MEDIA] Filtering {} total rules for element id='{}' type='{}'", rules_.size(), shape_id, shape_type);
    for (const auto& rule : rules_) {
        bool matches = evaluate_media_query(rule.media_query);
        if (matches) {
            filtered_rules.push_back(rule);
            // logi("[MEDIA] ✓ Rule '{}' with media='{}' INCLUDED", rule.selector, rule.media_query);
        } else {
            // logi("[MEDIA] ✗ Rule '{}' with media='{}' EXCLUDED", rule.selector, rule.media_query);
        }
    }
    // logi("[MEDIA] Filtered result: {}/{} rules passed", filtered_rules.size(), rules_.size());

    // Pass filtered rules to the computer
    auto result = computer_.compute_style(
        parser_.get_stylesheet(),
        filtered_rules,  // Pass filtered rules!
        shape_id, shape_type, classes, attributes, pseudo_states,
        inline_style, parent_style
    );

    // Resolve CSS variables in the computed style
    for (auto& [key, value] : result) {
        value = variable_resolver_.resolve(value);
    }

    // DEBUG: Log computed style for critical properties
    // if (result.count("width") > 0) {
    //     logi("[STYLE] Computed width='{}' for element id='{}' type='{}'", result["width"], shape_id, shape_type);
    // }
    // if (result.count("flex-direction") > 0) {
    //     logi("[STYLE] Computed flex-direction='{}' for element id='{}' type='{}'", result["flex-direction"], shape_id, shape_type);
    // }

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
    // Create empty set for cycle detection
    std::set<std::string> resolving_vars;
    return resolve_recursive(value, resolving_vars);
}

std::string CSSVariableResolver::resolve_recursive(const std::string& value, std::set<std::string>& resolving_vars) const {
    // If the value doesn't contain "var(", no need to resolve.
    if (value.find("var(") == std::string::npos) {
        return value;
    }

    // logi("[CSS VAR] Resolving value: '{}'", value);
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

        // Resolve it with cycle detection
        std::string resolved = resolve_var(var_expr, resolving_vars);

        // logi("[CSS VAR]   Expression: '{}' -> Resolved: '{}'", var_expr, resolved);

        // Replace in result
        result.replace(start, i - start, resolved);

        // Continue from after the replacement
        pos = start + resolved.size();
    }

    // logi("[CSS VAR] Final resolved value: '{}'", result);
    return result;
}

CSSVariableResolver CSSVariableResolver::create_scope() const {
    return CSSVariableResolver(this);
}

void CSSVariableResolver::clear() {
    variables_.clear();
}

std::string CSSVariableResolver::resolve_var(const std::string& expr, std::set<std::string>& resolving_vars) const {
    std::string var_name;
    std::string fallback;

    if (!parse_var_expression(expr, var_name, fallback)) {
        // Invalid syntax - return as-is
        return expr;
    }

    // Check for circular reference
    if (resolving_vars.count(var_name) > 0) {
        logi("[CSS VAR] Circular reference detected for variable: '{}'", var_name);
        // Return original expression to prevent infinite loop
        return expr;
    }

    // Check if variable exists
    if (has_variable(var_name)) {
        // Add to resolving set
        resolving_vars.insert(var_name);

        // Get variable value
        std::string value = get_variable(var_name);

        // Recursively resolve the value (it might contain var() too)
        std::string resolved = resolve_recursive(value, resolving_vars);

        // Remove from resolving set
        resolving_vars.erase(var_name);

        return resolved;
    }

    // Variable doesn't exist - use fallback
    if (!fallback.empty()) {
        // Recursively resolve fallback (it might contain var() too)
        return resolve_recursive(fallback, resolving_vars);
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

cssbox::ComputedStyle EnhancedStyleSheet::compute_style_typed(
    const std::string& shape_id,
    const std::string& shape_type,
    const std::vector<std::string>& classes,
    const std::map<std::string, std::string>& attributes,
    const std::set<std::string>& pseudo_states,
    const std::map<std::string, std::string>& inline_style,
    const cssbox::ComputedStyle* parent_style,
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
    cssbox::ComputedStyle result;

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
    result.display = cssbox::convert::parse_display(get("display"));
    result.position = cssbox::convert::parse_position(get("position"));
    result.box_sizing = cssbox::convert::parse_box_sizing(get("box-sizing"));

    // === Dimensions ===
    if (auto w = cssbox::convert::parse_length(get("width"))) result.width = *w;
    if (auto h = cssbox::convert::parse_length(get("height"))) result.height = *h;
    if (auto mw = cssbox::convert::parse_length(get("min-width"))) result.min_width = *mw;
    if (auto mh = cssbox::convert::parse_length(get("min-height"))) result.min_height = *mh;
    if (auto mxw = cssbox::convert::parse_length(get("max-width"))) result.max_width = *mxw;
    if (auto mxh = cssbox::convert::parse_length(get("max-height"))) result.max_height = *mxh;

    // === Position Offsets ===
    if (auto t = cssbox::convert::parse_length(get("top"))) result.top = *t;
    if (auto r = cssbox::convert::parse_length(get("right"))) result.right = *r;
    if (auto b = cssbox::convert::parse_length(get("bottom"))) result.bottom = *b;
    if (auto l = cssbox::convert::parse_length(get("left"))) result.left = *l;

    // Parse z-index
    std::string z = get("z-index");
    if (!z.empty() && z != "auto") {
        result.z_index = std::atoi(z.c_str());
    }

    // === Box Model ===
    // Padding (shorthand or individual sides)
    std::string padding = get("padding");
    if (!padding.empty()) {
        result.padding = cssbox::convert::parse_box_sides(padding);
    } else {
        // Individual sides
        if (auto pt = cssbox::convert::parse_length(get("padding-top"))) result.padding[0] = *pt;
        if (auto pr = cssbox::convert::parse_length(get("padding-right"))) result.padding[1] = *pr;
        if (auto pb = cssbox::convert::parse_length(get("padding-bottom"))) result.padding[2] = *pb;
        if (auto pl = cssbox::convert::parse_length(get("padding-left"))) result.padding[3] = *pl;
    }

    // Margin (shorthand or individual sides)
    std::string margin = get("margin");
    if (!margin.empty()) {
        result.margin = cssbox::convert::parse_box_sides(margin);
    } else {
        if (auto mt = cssbox::convert::parse_length(get("margin-top"))) result.margin[0] = *mt;
        if (auto mr = cssbox::convert::parse_length(get("margin-right"))) result.margin[1] = *mr;
        if (auto mb = cssbox::convert::parse_length(get("margin-bottom"))) result.margin[2] = *mb;
        if (auto ml = cssbox::convert::parse_length(get("margin-left"))) result.margin[3] = *ml;
    }

    // Border
    // Border width
    std::string border_width_str = get("border-width");
    if (!border_width_str.empty()) {
        auto widths = cssbox::convert::parse_box_sides(border_width_str);
        for (int i = 0; i < 4; ++i) {
            result.border.width[i] = widths[i].resolve(0, 16, 800);  // Convert to pixels
        }
    } else {
        if (auto btw = cssbox::convert::parse_length(get("border-top-width")))
            result.border.width[0] = btw->resolve(0, 16, 800);
        if (auto brw = cssbox::convert::parse_length(get("border-right-width")))
            result.border.width[1] = brw->resolve(0, 16, 800);
        if (auto bbw = cssbox::convert::parse_length(get("border-bottom-width")))
            result.border.width[2] = bbw->resolve(0, 16, 800);
        if (auto blw = cssbox::convert::parse_length(get("border-left-width")))
            result.border.width[3] = blw->resolve(0, 16, 800);
    }

    // Border style
    result.border.style[0] = cssbox::convert::parse_border_style(get("border-top-style"));
    result.border.style[1] = cssbox::convert::parse_border_style(get("border-right-style"));
    result.border.style[2] = cssbox::convert::parse_border_style(get("border-bottom-style"));
    result.border.style[3] = cssbox::convert::parse_border_style(get("border-left-style"));

    // Border color
    if (auto btc = cssbox::convert::parse_color(get("border-top-color")))
        result.border.color[0] = *btc;
    if (auto brc = cssbox::convert::parse_color(get("border-right-color")))
        result.border.color[1] = *brc;
    if (auto bbc = cssbox::convert::parse_color(get("border-bottom-color")))
        result.border.color[2] = *bbc;
    if (auto blc = cssbox::convert::parse_color(get("border-left-color")))
        result.border.color[3] = *blc;

    // Border radius
    std::string border_radius_str = get("border-radius");
    if (!border_radius_str.empty()) {
        auto radii = cssbox::convert::parse_box_sides(border_radius_str);
        for (int i = 0; i < 4; ++i) {
            // Don't resolve percentages here - mark with negative value
            // Layout engine will resolve based on element dimensions
            if (radii[i].unit == cssbox::LengthUnit::PERCENT) {
                result.border.radius[i] = -radii[i].value;  // Store as negative percentage
            } else {
                result.border.radius[i] = radii[i].resolve(0, 16, 800);
            }
        }
    }

    // === Overflow ===
    result.overflow_x = cssbox::convert::parse_overflow(get("overflow-x"));
    result.overflow_y = cssbox::convert::parse_overflow(get("overflow-y"));
    std::string overflow = get("overflow");
    if (!overflow.empty()) {
        auto o = cssbox::convert::parse_overflow(overflow);
        result.overflow_x = result.overflow_y = o;
    }

    // === Background ===
    std::string bg_str = get("background");
    std::string bg_color_str = get("background-color");
    
    // Only override background if CSS provides a non-transparent value
    bool has_bg_css = false;
    
    // Check background-color first (most specific)
    if (!bg_color_str.empty() && bg_color_str != "transparent") {
        if (auto c = cssbox::convert::parse_color(bg_color_str)) {
            result.background = cssbox::Background::solid(*c);
            has_bg_css = true;
        }
    }
    // Then check background shorthand (can be gradient or color)
    else if (!bg_str.empty() && bg_str != "transparent") {
        // Check if it's a gradient
        if (bg_str.find("gradient") != std::string::npos) {
            // Store gradient string - painter will parse it
            result.background.type = cssbox::BackgroundType::GRADIENT;
            result.background.gradient_css = bg_str;
            has_bg_css = true;
        }
        // Try to parse as color
        else if (auto c = cssbox::convert::parse_color(bg_str)) {
            result.background = cssbox::Background::solid(*c);
            has_bg_css = true;
        }
    }
    
    // If no CSS background was set, result.background remains at its default (transparent)
    // The caller (cssboxUpdate) should preserve existing element->style.background if result is transparent

    // Opacity
    std::string opacity_str = get("opacity");
    if (!opacity_str.empty()) {
        result.opacity = std::strtof(opacity_str.c_str(), nullptr);
    }

    // === Flexbox ===
    // Only parse if value is non-empty to preserve typed struct defaults
    std::string flex_direction_str = get("flex-direction");
    if (!flex_direction_str.empty()) result.flex_direction = cssbox::convert::parse_flex_direction(flex_direction_str);

    std::string flex_wrap_str = get("flex-wrap");
    if (!flex_wrap_str.empty()) result.flex_wrap = cssbox::convert::parse_flex_wrap(flex_wrap_str);

    std::string justify_content_str = get("justify-content");
    if (!justify_content_str.empty()) result.justify_content = cssbox::convert::parse_justify_content(justify_content_str);

    std::string align_items_str = get("align-items");
    if (!align_items_str.empty()) result.align_items = cssbox::convert::parse_align_items(align_items_str);

    std::string align_content_str = get("align-content");
    if (!align_content_str.empty()) result.align_content = cssbox::convert::parse_align_content(align_content_str);

    std::string flex_grow_str = get("flex-grow");
    if (!flex_grow_str.empty()) result.flex_grow = std::strtof(flex_grow_str.c_str(), nullptr);

    std::string flex_shrink_str = get("flex-shrink");
    if (!flex_shrink_str.empty()) result.flex_shrink = std::strtof(flex_shrink_str.c_str(), nullptr);

    if (auto fb = cssbox::convert::parse_length(get("flex-basis"))) result.flex_basis = *fb;

    std::string order_str = get("order");
    if (!order_str.empty()) result.order = std::atoi(order_str.c_str());

    if (auto g = cssbox::convert::parse_length(get("gap"))) result.gap = *g;

    // === Text ===
    if (auto c = cssbox::convert::parse_color(get("color"))) result.color = *c;

    std::string font_size_str = get("font-size");
    if (!font_size_str.empty()) {
        if (auto fs = cssbox::convert::parse_length(font_size_str)) {
            result.font_size = fs->resolve(16.0f, 16.0f, 800.0f);  // Resolve to pixels
        }
    }

    result.font_weight = cssbox::convert::parse_font_weight(get("font-weight"));
    result.font_style = cssbox::convert::parse_font_style(get("font-style"));
    result.text_align = cssbox::convert::parse_text_align(get("text-align"));
    result.vertical_align = cssbox::convert::parse_vertical_align(get("vertical-align"));
    result.text_decoration = cssbox::convert::parse_text_decoration(get("text-decoration"));

    std::string font_family_str = get("font-family");
    if (!font_family_str.empty()) {
        result.font_family = font_family_str;
    }

    // === Grid Properties ===
    std::string grid_template_rows_str = get("grid-template-rows");
    if (!grid_template_rows_str.empty()) {
        result.grid_template_rows = cssbox::convert::parse_grid_track_list(grid_template_rows_str);
    }

    std::string grid_template_columns_str = get("grid-template-columns");
    if (!grid_template_columns_str.empty()) {
        result.grid_template_columns = cssbox::convert::parse_grid_track_list(grid_template_columns_str);
    }

    if (auto row_gap = cssbox::convert::parse_length(get("grid-row-gap"))) {
        result.grid_row_gap = *row_gap;
    } else if (auto row_gap2 = cssbox::convert::parse_length(get("row-gap"))) {
        result.grid_row_gap = *row_gap2;
    }

    if (auto col_gap = cssbox::convert::parse_length(get("grid-column-gap"))) {
        result.grid_column_gap = *col_gap;
    } else if (auto col_gap2 = cssbox::convert::parse_length(get("column-gap"))) {
        result.grid_column_gap = *col_gap2;
    }

    // Additional properties (box-shadows, text-shadows, transforms) are handled
    // by the string-based system and converted during rendering

    // === SVG Stroke Properties (Rough Rendering) ===
    std::string stroke_rendering_str = get("stroke-rendering");
    if (!stroke_rendering_str.empty()) {
        if (stroke_rendering_str == "rough") {
            result.svg_stroke.rendering = cssbox::StrokeRendering::ROUGH;
        } else {
            result.svg_stroke.rendering = cssbox::StrokeRendering::AUTO;
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

    // === SVG Fill Property ===
    std::string fill_str = get("fill");
    if (!fill_str.empty()) {
        if (fill_str == "none") {
            result.svg_fill.enabled = false;
        } else {
            if (auto fill_color = cssbox::convert::parse_color(fill_str)) {
                result.svg_fill.color = *fill_color;
                result.svg_fill.enabled = true;
            }
        }
    }

    return result;
}

bool EnhancedStyleSheet::evaluate_media_query(const std::string& media_query) const {
    // Empty media query always matches
    if (media_query.empty()) {
        return true;
    }

    logi("[MEDIA] Evaluating query: '{}' with viewport {}x{}", media_query, viewport_width_, viewport_height_);

    // Simple parser for common media queries: (min-width: Xpx), (max-width: Xpx), etc.
    std::string query = media_query;

    // Remove outer parentheses if present
    if (!query.empty() && query.front() == '(' && query.back() == ')') {
        query = query.substr(1, query.size() - 2);
    }

    // Parse "feature: value" pairs
    size_t colon_pos = query.find(':');
    if (colon_pos == std::string::npos) {
        logi("[MEDIA] Malformed query, assuming match");
        return true;  // Malformed query, assume it matches
    }

    std::string feature = query.substr(0, colon_pos);
    std::string value_str = query.substr(colon_pos + 1);

    // Trim whitespace
    feature.erase(0, feature.find_first_not_of(" \t"));
    feature.erase(feature.find_last_not_of(" \t") + 1);
    value_str.erase(0, value_str.find_first_not_of(" \t"));
    value_str.erase(value_str.find_last_not_of(" \t") + 1);

    // Parse value (remove "px" suffix if present)
    float value = 0.0f;
    if (value_str.size() >= 2 && value_str.substr(value_str.size() - 2) == "px") {
        value = std::strtof(value_str.substr(0, value_str.size() - 2).c_str(), nullptr);
    } else {
        value = std::strtof(value_str.c_str(), nullptr);
    }

    logi("[MEDIA] Parsed: feature='{}', value={}", feature, value);

    // Evaluate feature
    bool result = true;
    if (feature == "min-width") {
        result = viewport_width_ >= value;
    } else if (feature == "max-width") {
        result = viewport_width_ <= value;
    } else if (feature == "min-height") {
        result = viewport_height_ >= value;
    } else if (feature == "max-height") {
        result = viewport_height_ <= value;
    }

    logi("[MEDIA] Query result: {}", result);
    return result;
}

} // namespace lexbor
} // namespace cssbox

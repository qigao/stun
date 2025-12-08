/*
 * NanoVG CSS - Simple Stylesheet Implementation
 *
 * Phase 1: Simple CSS parsing and selector matching without lexbor.
 */

#include "cssbox_internal.h"
#include "css_keyword_lexer.h"
#include <algorithm>
#include <cctype>
#include <sstream>

// ============================================================================
// SimpleStyleSheet Implementation
// ============================================================================

bool SimpleStyleSheet::parse_css(const std::string& css) {
    // Simple CSS parser for Phase 1
    // Handles rules like: .class { property: value; }

    rules_.clear();

    // First, remove all comments
    std::string cleaned_css;
    size_t i = 0;
    while (i < css.length()) {
        if (i + 1 < css.length() && css[i] == '/' && css[i + 1] == '*') {
            // Start of comment, skip until */
            i += 2;
            while (i + 1 < css.length()) {
                if (css[i] == '*' && css[i + 1] == '/') {
                    i += 2;
                    break;
                }
                i++;
            }
        } else {
            cleaned_css += css[i];
            i++;
        }
    }

    size_t pos = 0;
    while (pos < cleaned_css.length()) {
        // Skip whitespace
        while (pos < cleaned_css.length() && std::isspace(cleaned_css[pos])) {
            pos++;
        }

        if (pos >= cleaned_css.length()) break;

        // Check for @keyframes rule (Phase 4 Sprint 2)
        if (pos + 10 <= cleaned_css.length() && cleaned_css.substr(pos, 10) == "@keyframes") {
            pos += 10;

            // Skip whitespace
            while (pos < cleaned_css.length() && std::isspace(cleaned_css[pos])) {
                pos++;
            }

            // Find animation name (everything before {)
            size_t name_start = pos;
            size_t brace_open = cleaned_css.find('{', pos);
            if (brace_open == std::string::npos) break;

            std::string anim_name = cleaned_css.substr(name_start, brace_open - name_start);
            anim_name.erase(0, anim_name.find_first_not_of(" \t\n\r"));
            anim_name.erase(anim_name.find_last_not_of(" \t\n\r") + 1);

            // Find matching closing brace (need to handle nested braces)
            int brace_count = 1;
            size_t brace_close = brace_open + 1;
            while (brace_close < cleaned_css.length() && brace_count > 0) {
                if (cleaned_css[brace_close] == '{') brace_count++;
                else if (cleaned_css[brace_close] == '}') brace_count--;
                brace_close++;
            }
            brace_close--;  // Point to the closing brace

            // Extract keyframes content
            std::string keyframes_content = cleaned_css.substr(brace_open + 1, brace_close - brace_open - 1);

            // Parse keyframes (call helper function)
            KeyframeAnimation animation = parse_keyframes_rule(keyframes_content, anim_name);
            add_keyframe_animation(animation);

            // Move past this rule
            pos = brace_close + 1;
            continue;
        }

        // Find selector (everything before {)
        size_t selector_start = pos;
        size_t brace_open = cleaned_css.find('{', pos);
        if (brace_open == std::string::npos) break;

        std::string selector = cleaned_css.substr(selector_start, brace_open - selector_start);

        // Trim selector
        selector.erase(0, selector.find_first_not_of(" \t\n\r"));
        selector.erase(selector.find_last_not_of(" \t\n\r") + 1);

        // Find properties (everything between { and })
        size_t brace_close = cleaned_css.find('}', brace_open);
        if (brace_close == std::string::npos) break;

        std::string properties_str = cleaned_css.substr(brace_open + 1, brace_close - brace_open - 1);

        // Parse properties - split by semicolons, not newlines
        std::map<std::string, std::string> properties;

        size_t prop_pos = 0;
        while (prop_pos < properties_str.length()) {
            // Find next semicolon
            size_t semi_pos = properties_str.find(';', prop_pos);
            std::string declaration;

            if (semi_pos != std::string::npos) {
                declaration = properties_str.substr(prop_pos, semi_pos - prop_pos);
                prop_pos = semi_pos + 1;
            } else {
                declaration = properties_str.substr(prop_pos);
                prop_pos = properties_str.length();
            }

            // Find colon in declaration
            size_t colon_pos = declaration.find(':');
            if (colon_pos == std::string::npos) continue;

            std::string prop = declaration.substr(0, colon_pos);
            std::string value = declaration.substr(colon_pos + 1);

            // Trim
            prop.erase(0, prop.find_first_not_of(" \t\n\r"));
            prop.erase(prop.find_last_not_of(" \t\n\r") + 1);
            value.erase(0, value.find_first_not_of(" \t\n\r"));
            value.erase(value.find_last_not_of(" \t\n\r") + 1);

            // Normalize property name to lowercase
            std::transform(prop.begin(), prop.end(), prop.begin(),
                           [](unsigned char c){ return std::tolower(c); });

            if (!prop.empty() && !value.empty()) {
                // Sprint 23: Check if this is a CSS variable definition (starts with --)
                if (prop.length() >= 2 && prop.substr(0, 2) == "--") {
                    // Store as CSS variable
                    set_variable(prop, value);
                } else {
                    properties[prop] = value;
                }

                // Expand shorthand properties
                if (prop == "border") {
                    // Parse border shorthand: "2px solid red"
                    // Can be in any order: width style color
                    std::istringstream border_stream(value);
                    std::string part1, part2, part3;
                    border_stream >> part1 >> part2 >> part3;

                    // Try to identify each part
                    std::string width, style, color;

                    for (const auto& part : {part1, part2, part3}) {
                        if (part.empty()) continue;

                        // Check if it's a width (ends with px, em, etc)
                        if (part.find("px") != std::string::npos ||
                            part.find("em") != std::string::npos ||
                            part.find("rem") != std::string::npos) {
                            width = part;
                        }
                        // Check if it's a style keyword (use DFA for fast matching)
                        else if (cssbox::fast::parse_border_style(part) != CSS_BORDER_STYLE_UNKNOWN) {
                            style = part;
                        }
                        // Otherwise it's probably a color
                        else {
                            color = part;
                        }
                    }

                    // Add expanded properties
                    if (!width.empty()) properties["border-width"] = width;
                    if (!style.empty()) properties["border-style"] = style;
                    if (!color.empty()) properties["border-color"] = color;
                }
            }
        }

        // Add rule(s) - split comma-separated selectors
        if (!properties.empty()) {
            // Split by comma to handle "button, .button { ... }"
            size_t comma_pos = 0;
            size_t start = 0;
            while (start < selector.length()) {
                comma_pos = selector.find(',', start);
                if (comma_pos == std::string::npos) {
                    // Last selector
                    std::string single_selector = selector.substr(start);
                    single_selector.erase(0, single_selector.find_first_not_of(" \t\n\r"));
                    single_selector.erase(single_selector.find_last_not_of(" \t\n\r") + 1);
                    if (!single_selector.empty()) {
                        add_rule(single_selector, properties);
                    }
                    break;
                } else {
                    // Found a comma, extract this selector
                    std::string single_selector = selector.substr(start, comma_pos - start);
                    single_selector.erase(0, single_selector.find_first_not_of(" \t\n\r"));
                    single_selector.erase(single_selector.find_last_not_of(" \t\n\r") + 1);
                    if (!single_selector.empty()) {
                        add_rule(single_selector, properties);
                    }
                    start = comma_pos + 1;
                }
            }
        }

        pos = brace_close + 1;
    }

    return true;
}

void SimpleStyleSheet::add_rule(const std::string& selector,
                                const std::map<std::string, std::string>& properties) {
    CSSRule rule;
    rule.selector = selector;
    rule.properties = properties;
    rule.specificity = calculate_specificity(selector);
    rules_.push_back(rule);

    // Sort rules by specificity (higher specificity last, so they override)
    std::sort(rules_.begin(), rules_.end(),
        [](const CSSRule& a, const CSSRule& b) {
            return a.specificity < b.specificity;
        });
}

// Sprint 30: Nth-child pattern parser
struct NthChildPattern {
    int a;  // Coefficient (e.g., 2 in "2n+1")
    int b;  // Offset (e.g., 1 in "2n+1")

    bool matches(int child_index) const {
        // child_index is 0-based, CSS nth-child is 1-based
        int n = child_index + 1;

        if (a == 0) {
            // :nth-child(b) - exact match
            return n == b;
        } else {
            // :nth-child(an+b)
            if (a == 0) return false;
            if ((n - b) % a != 0) return false;
            int k = (n - b) / a;
            return k >= 0;
        }
    }
};

// Sprint 30: Parse nth-child expression (optimized with re2c for odd/even)
static NthChildPattern parse_nth_child(const std::string& expr) {
    std::string str = expr;
    str.erase(0, str.find_first_not_of(" \t"));
    str.erase(str.find_last_not_of(" \t") + 1);

    NthChildPattern result = {0, 0};

    // Use DFA for odd/even keywords
    switch (cssbox::fast::parse_nth_keyword(str)) {
        case CSS_NTH_ODD:
            result.a = 2;
            result.b = 1;
            return result;
        case CSS_NTH_EVEN:
            result.a = 2;
            result.b = 0;
            return result;
    }

    size_t n_pos = str.find('n');
    if (n_pos == std::string::npos) {
        result.a = 0;
        result.b = std::stoi(str);
    } else {
        std::string a_str = str.substr(0, n_pos);
        a_str.erase(0, a_str.find_first_not_of(" 	"));
        a_str.erase(a_str.find_last_not_of(" 	") + 1);

        if (a_str.empty() || a_str == "+") {
            result.a = 1;
        } else if (a_str == "-") {
            result.a = -1;
        } else {
            result.a = std::stoi(a_str);
        }

        if (n_pos + 1 < str.length()) {
            std::string b_str = str.substr(n_pos + 1);
            b_str.erase(0, b_str.find_first_not_of(" 	"));
            b_str.erase(b_str.find_last_not_of(" 	") + 1);
            if (!b_str.empty()) {
                result.b = std::stoi(b_str);
            }
        }
    }

    return result;
}

// Sprint 30: Check if pseudo-class matches (optimized with re2c DFA)
static bool match_structural_pseudo(const std::string& pseudo, int child_index, int total_siblings) {
    // Check for nth-child() with expression first (has parentheses)
    if (pseudo.length() > 11 && pseudo[9] == '(') {
        // Extract the base pseudo-class name (before the parenthesis)
        std::string_view base(pseudo.data(), 9);
        if (cssbox::fast::parse_structural_pseudo(base) == CSS_PSEUDO_NTH_CHILD) {
            size_t close = pseudo.find(')');
            if (close == std::string::npos) return false;
            std::string expr = pseudo.substr(10, close - 10);
            NthChildPattern pattern = parse_nth_child(expr);
            return pattern.matches(child_index);
        }
    }
    
    // Use DFA for simple structural pseudo-classes
    switch (cssbox::fast::parse_structural_pseudo(pseudo)) {
        case CSS_PSEUDO_FIRST_CHILD:
            return child_index == 0;
        case CSS_PSEUDO_LAST_CHILD:
            return child_index == total_siblings - 1;
        case CSS_PSEUDO_ONLY_CHILD:
            return total_siblings == 1;
        default:
            return false;
    }
}

std::map<std::string, std::string> SimpleStyleSheet::compute_style(
    const std::string& id,
    const std::string& type,
    const std::vector<std::string>& classes,
    const std::map<std::string, std::string>& attributes,
    const std::set<std::string>& pseudo_states,
    const std::map<std::string, std::string>& inline_style,
    const std::map<std::string, std::string>& parent_style,
    int child_index,
    int total_siblings) {

    std::map<std::string, std::string> result;
    int matched = 0;

    // Phase 1: No inheritance support yet (parent_style ignored)

    // Apply matching rules in specificity order
    for (const auto& rule : rules_) {
        if (matches_selector(rule.selector, id, type, classes, pseudo_states, child_index, total_siblings)) {
            matched++;
            // Apply properties
            for (const auto& [prop, value] : rule.properties) {
                result[prop] = resolve_variables(value);
            }
        }
    }

    // Apply inline styles (highest priority)
    for (const auto& [prop, value] : inline_style) {
        result[prop] = resolve_variables(value);
    }

    return result;
}

void SimpleStyleSheet::set_variable(const std::string& name, const std::string& value) {
    variables_[name] = value;
}

bool SimpleStyleSheet::matches_selector(const std::string& selector,
                                       const std::string& id,
                                       const std::string& type,
                                       const std::vector<std::string>& classes,
                                       const std::set<std::string>& pseudo_states,
                                       int child_index,
                                       int total_siblings) const {
    // Phase 1: Simple selector matching
    // Supports: .class, #id, type, :pseudo-class

    std::string sel = selector;

    // Trim
    sel.erase(0, sel.find_first_not_of(" \t\n\r"));
    sel.erase(sel.find_last_not_of(" \t\n\r") + 1);

    // Check for compound selectors (e.g., "rect.selected")
    // For now, just check if all parts match

    // ID selector
    if (sel[0] == '#') {
        std::string sel_id = sel.substr(1);
        return sel_id == id;
    }

    // Class selector
    if (sel[0] == '.') {
        std::string sel_class = sel.substr(1);
        // Check for compound with pseudo (e.g., ".box:hover")
        size_t colon_pos = sel_class.find(':');
        if (colon_pos != std::string::npos) {
            // Extract class part before colon
            std::string class_part = sel_class.substr(0, colon_pos);
            std::string pseudo_part = sel_class.substr(colon_pos + 1);

            bool class_matches = std::find(classes.begin(), classes.end(), class_part) != classes.end();
            // Sprint 30: Check structural pseudo-classes first
            bool pseudo_matches = match_structural_pseudo(pseudo_part, child_index, total_siblings) ||
                                  pseudo_states.count(pseudo_part) > 0;

            bool result = class_matches && pseudo_matches;
            return result;
        }

        bool result = std::find(classes.begin(), classes.end(), sel_class) != classes.end();
        return result;
    }

    // Pseudo-class selector
    if (sel[0] == ':') {
        std::string pseudo = sel.substr(1);
        // Sprint 30: Check structural pseudo-classes first
        if (match_structural_pseudo(pseudo, child_index, total_siblings)) {
            return true;
        }
        // Check regular pseudo-states (hover, active, focus)
        return pseudo_states.count(pseudo) > 0;
    }

    // Type selector or compound
    if (sel.find('.') != std::string::npos || sel.find(':') != std::string::npos) {
        // Compound selector like "rect.selected" or "rect:hover"
        bool type_match = true;
        bool class_match = true;
        bool pseudo_match = true;

        // Extract type
        size_t dot_pos = sel.find('.');
        size_t colon_pos = sel.find(':');
        size_t first_special = std::min(
            dot_pos == std::string::npos ? sel.length() : dot_pos,
            colon_pos == std::string::npos ? sel.length() : colon_pos
        );

        if (first_special > 0) {
            std::string sel_type = sel.substr(0, first_special);
            type_match = (sel_type == type || sel_type == "*");
        }

        // Extract classes
        if (dot_pos != std::string::npos) {
            size_t end_pos = colon_pos != std::string::npos ? colon_pos : sel.length();
            std::string sel_class = sel.substr(dot_pos + 1, end_pos - dot_pos - 1);
            class_match = std::find(classes.begin(), classes.end(), sel_class) != classes.end();
        }

        // Extract pseudo-class
        if (colon_pos != std::string::npos) {
            std::string pseudo = sel.substr(colon_pos + 1);
            // Sprint 30: Check structural pseudo-classes first
            pseudo_match = match_structural_pseudo(pseudo, child_index, total_siblings) ||
                           pseudo_states.count(pseudo) > 0;
        }

        return type_match && class_match && pseudo_match;
    }

    // Simple type selector
    return sel == type || sel == "*";
}

int SimpleStyleSheet::calculate_specificity(const std::string& selector) const {
    // Simplified specificity calculation
    // Phase 1: a-b-c format where a=IDs, b=classes+pseudo, c=types

    int specificity = 0;

    // Count IDs (100 each)
    size_t pos = 0;
    while ((pos = selector.find('#', pos)) != std::string::npos) {
        specificity += 100;
        pos++;
    }

    // Count classes (10 each)
    pos = 0;
    while ((pos = selector.find('.', pos)) != std::string::npos) {
        specificity += 10;
        pos++;
    }

    // Count pseudo-classes (10 each)
    pos = 0;
    while ((pos = selector.find(':', pos)) != std::string::npos) {
        specificity += 10;
        pos++;
    }

    // Type selectors (1 each) - assume at least one if not universal
    if (selector.find('*') == std::string::npos &&
        selector.find('#') != 0 &&
        selector.find('.') != 0) {
        specificity += 1;
    }

    return specificity;
}

std::string SimpleStyleSheet::resolve_variables(const std::string& value) const {
    // Simple var() resolution: var(--name) or var(--name, fallback)
    std::string result = value;

    size_t var_pos = result.find("var(");
    while (var_pos != std::string::npos) {
        size_t close_paren = result.find(')', var_pos);
        if (close_paren == std::string::npos) break;

        std::string var_content = result.substr(var_pos + 4, close_paren - var_pos - 4);

        // Check for fallback
        size_t comma_pos = var_content.find(',');
        std::string var_name, fallback;

        if (comma_pos != std::string::npos) {
            var_name = var_content.substr(0, comma_pos);
            fallback = var_content.substr(comma_pos + 1);

            // Trim
            var_name.erase(0, var_name.find_first_not_of(" \t"));
            var_name.erase(var_name.find_last_not_of(" \t") + 1);
            fallback.erase(0, fallback.find_first_not_of(" \t"));
            fallback.erase(fallback.find_last_not_of(" \t") + 1);
        } else {
            var_name = var_content;
            var_name.erase(0, var_name.find_first_not_of(" \t"));
            var_name.erase(var_name.find_last_not_of(" \t") + 1);
        }

        // Resolve variable
        std::string resolved;
        auto it = variables_.find(var_name);
        if (it != variables_.end()) {
            resolved = it->second;
        } else if (!fallback.empty()) {
            resolved = fallback;
        } else {
            resolved = ""; // Unresolved
        }

        // Replace var() with resolved value
        result.replace(var_pos, close_paren - var_pos + 1, resolved);

        // Continue searching
        var_pos = result.find("var(", var_pos);
    }

    return result;
}

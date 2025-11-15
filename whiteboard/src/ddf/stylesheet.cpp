#include <whiteboard/ddf/stylesheet.h>
#include <whiteboard/lexbor/lexbor_css_parser.h>
#include <algorithm>
#include <sstream>

// Bring Lexbor classes into whiteboard namespace for convenience
using whiteboard::lexbor::LexborCSSParser;
using whiteboard::lexbor::LexborSelectorMatcher;
using whiteboard::lexbor::LexborStyleComputer;

namespace whiteboard {
namespace ddf {

StyleSheet::StyleSheet() : use_lexbor_(true) {
    // Initialize Lexbor components
    lexbor_parser_ = std::make_unique<LexborCSSParser>();
    lexbor_matcher_ = std::make_unique<LexborSelectorMatcher>();
    lexbor_computer_ = std::make_unique<LexborStyleComputer>();
}

StyleSheet::~StyleSheet() = default;

void StyleSheet::add_rule(const std::string& selector,
                          const std::map<std::string, std::string>& properties) {
    int spec = calculate_specificity(selector);
    rules_.push_back(StyleRule(selector, properties, spec));
}

bool StyleSheet::parse_css(const std::string& css_text) {
    if (use_lexbor_ && lexbor_parser_) {
        // Use Lexbor to parse CSS
        return lexbor_parser_->parse(css_text);
    } else {
        // Legacy: CSS parsing not supported in old version
        return false;
    }
}

std::vector<StyleRule> StyleSheet::get_matching_rules(
    const std::string& shape_id,
    const std::string& shape_type,
    const std::vector<std::string>& classes,
    const std::set<std::string>& pseudo_states) const {
    
    std::vector<StyleRule> matching;
    
    for (const auto& rule : rules_) {
        if (matches_selector(rule.selector, shape_id, shape_type, classes, pseudo_states)) {
            matching.push_back(rule);
        }
    }
    
    // Sort by specificity (lower first, so higher overwrites)
    std::sort(matching.begin(), matching.end(),
              [](const StyleRule& a, const StyleRule& b) {
                  return a.specificity < b.specificity;
              });
    
    return matching;
}

std::map<std::string, std::string> StyleSheet::compute_style(
    const std::string& shape_id,
    const std::string& shape_type,
    const std::vector<std::string>& classes,
    const std::set<std::string>& pseudo_states,
    const std::map<std::string, std::string>& inline_style,
    const std::map<std::string, std::string>& parent_style) const {
    
    // If Lexbor is enabled and we have a parsed stylesheet, use it
    if (use_lexbor_ && lexbor_computer_ && lexbor_parser_ && lexbor_parser_->get_stylesheet()) {
        // Convert pseudo_states to attributes map (empty for now)
        std::map<std::string, std::string> attributes;
        
        // Use Lexbor to compute styles
        return lexbor_computer_->compute_style(
            lexbor_parser_->get_stylesheet(),
            shape_id,
            shape_type,
            classes,
            attributes,
            pseudo_states,
            inline_style,
            parent_style
        );
    }
    
    // Fall back to legacy implementation
    
    // 1. Start with default styles
    std::map<std::string, std::string> computed = get_default_styles(shape_type);
    
    // 2. Inherit properties from parent
    for (const auto& [key, value] : parent_style) {
        if (is_inheritable(key)) {
            computed[key] = value;
        }
    }
    
    // 3. Apply matching rules (sorted by specificity)
    auto matching = get_matching_rules(shape_id, shape_type, classes, pseudo_states);
    for (const auto& rule : matching) {
        for (const auto& [key, value] : rule.properties) {
            computed[key] = value;
        }
    }
    
    // 4. Apply inline styles (highest priority)
    for (const auto& [key, value] : inline_style) {
        computed[key] = value;
    }
    
    return computed;
}

void StyleSheet::clear() {
    rules_.clear();
    if (lexbor_parser_) {
        // Reset Lexbor parser
        lexbor_parser_ = std::make_unique<LexborCSSParser>();
    }
}

bool StyleSheet::matches_selector(
    const std::string& selector,
    const std::string& shape_id,
    const std::string& shape_type,
    const std::vector<std::string>& classes,
    const std::set<std::string>& pseudo_states) const {
    
    if (selector.empty()) {
        return false;
    }
    
    // Parse selector for pseudo-class
    size_t colon_pos = selector.find(':');
    std::string base_selector = (colon_pos != std::string::npos) 
                                ? selector.substr(0, colon_pos) 
                                : selector;
    std::string pseudo = (colon_pos != std::string::npos) 
                        ? selector.substr(colon_pos + 1) 
                        : "";
    
    // Check base selector
    bool base_matches = false;
    
    if (base_selector.empty()) {
        // Universal selector or just pseudo-class
        base_matches = true;
    } else if (base_selector[0] == '#') {
        // ID selector: "#shape1"
        base_matches = (shape_id == base_selector.substr(1));
    } else if (base_selector[0] == '.') {
        // Class selector: ".highlight"
        std::string class_name = base_selector.substr(1);
        base_matches = std::find(classes.begin(), classes.end(), class_name) != classes.end();
    } else {
        // Type selector: "rect"
        base_matches = (shape_type == base_selector);
    }
    
    // Check pseudo-class
    bool pseudo_matches = true;
    if (!pseudo.empty()) {
        pseudo_matches = pseudo_states.count(pseudo) > 0;
    }
    
    return base_matches && pseudo_matches;
}

int StyleSheet::calculate_specificity(const std::string& selector) const {
    int specificity = 0;
    
    // Parse selector to separate base from pseudo
    size_t colon_pos = selector.find(':');
    std::string base_selector = (colon_pos != std::string::npos) 
                                ? selector.substr(0, colon_pos) 
                                : selector;
    bool has_pseudo = (colon_pos != std::string::npos);
    
    // Count specificity components
    int id_count = 0;
    int class_count = 0;
    int type_count = 0;
    
    if (!base_selector.empty()) {
        if (base_selector[0] == '#') {
            id_count = 1;
        } else if (base_selector[0] == '.') {
            class_count = 1;
        } else {
            type_count = 1;
        }
    }
    
    // Pseudo-classes count as class selectors
    if (has_pseudo) {
        class_count++;
    }
    
    // CSS specificity: id*100 + class*10 + type*1
    specificity = id_count * 100 + class_count * 10 + type_count;
    
    return specificity;
}

bool StyleSheet::is_inheritable(const std::string& property) const {
    // Properties that inherit from parent to child
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

std::map<std::string, std::string> StyleSheet::get_default_styles(const std::string& shape_type) const {
    std::map<std::string, std::string> defaults;
    
    // Common defaults for all shapes
    defaults["fill"] = "none";
    defaults["stroke"] = "black";
    defaults["stroke-width"] = "1";
    defaults["opacity"] = "1";
    
    // Type-specific defaults
    if (shape_type == "text") {
        defaults["font-family"] = "sans-serif";
        defaults["font-size"] = "14";
        defaults["font-weight"] = "normal";
        defaults["text-align"] = "left";
        defaults["fill"] = "black";
    } else if (shape_type == "rect" || shape_type == "circle" || 
               shape_type == "ellipse" || shape_type == "path") {
        defaults["fill"] = "#cccccc";
    }
    
    return defaults;
}

} // namespace ddf
} // namespace whiteboard

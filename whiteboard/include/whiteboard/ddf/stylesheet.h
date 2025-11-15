#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>

// Include Lexbor components (needed for unique_ptr)
#include <whiteboard/lexbor/lexbor_css_parser.h>

namespace whiteboard {
namespace ddf {

// Forward declaration
struct Shape;

/**
 * @brief Represents a CSS-like style rule
 */
struct StyleRule {
    std::string selector;  // ".class", "#id", "type", "type:pseudo"
    std::map<std::string, std::string> properties;
    int specificity;  // For cascade resolution
    
    StyleRule() : specificity(0) {}
    StyleRule(const std::string& sel, const std::map<std::string, std::string>& props, int spec)
        : selector(sel), properties(props), specificity(spec) {}
};

/**
 * @brief CSS-like stylesheet for DDF shapes
 * 
 * Now powered by Lexbor for full CSS3 support!
 * 
 * Supports:
 * - Type selectors (e.g., "rect", "circle")
 * - Class selectors (e.g., ".highlight")
 * - ID selectors (e.g., "#node1")
 * - Pseudo-class selectors (e.g., ":hover", ":selected")
 * - Multiple selectors (e.g., "rect, circle")
 * - Attribute selectors (e.g., "[data-status='active']")
 * - Complex selectors (e.g., ".card:not(.disabled)")
 * 
 * Specificity calculation (CSS-like):
 * - ID selector: 100
 * - Class/pseudo-class: 10
 * - Type selector: 1
 */
class StyleSheet {
public:
    StyleSheet();
    ~StyleSheet();

    /**
     * @brief Add a style rule to the stylesheet
     * @param selector CSS-like selector
     * @param properties Map of property name to value
     */
    void add_rule(const std::string& selector, 
                  const std::map<std::string, std::string>& properties);

    /**
     * @brief Parse CSS text and add rules to the stylesheet
     * @param css_text CSS text to parse
     * @return true if parsing succeeded, false otherwise
     */
    bool parse_css(const std::string& css_text);

    /**
     * @brief Get all rules that match a given shape
     * @param shape_id Shape ID
     * @param shape_type Shape type (e.g., "rect", "circle")
     * @param classes List of CSS classes
     * @param pseudo_states Set of pseudo-states (e.g., "hover", "selected")
     * @return Vector of matching rules sorted by specificity
     */
    std::vector<StyleRule> get_matching_rules(
        const std::string& shape_id,
        const std::string& shape_type,
        const std::vector<std::string>& classes,
        const std::set<std::string>& pseudo_states) const;

    /**
     * @brief Compute final style for a shape (cascade + inheritance)
     * @param shape_id Shape ID
     * @param shape_type Shape type
     * @param classes CSS classes
     * @param pseudo_states Pseudo-states
     * @param inline_style Inline styles (highest priority)
     * @param parent_style Parent's computed style (for inheritance)
     * @return Computed style map
     */
    std::map<std::string, std::string> compute_style(
        const std::string& shape_id,
        const std::string& shape_type,
        const std::vector<std::string>& classes,
        const std::set<std::string>& pseudo_states,
        const std::map<std::string, std::string>& inline_style,
        const std::map<std::string, std::string>& parent_style = {}) const;

    /**
     * @brief Get all rules in the stylesheet
     */
    const std::vector<StyleRule>& get_rules() const { return rules_; }

    /**
     * @brief Clear all rules
     */
    void clear();

    /**
     * @brief Check if using Lexbor parser
     * @return true if Lexbor is enabled, false if using legacy parser
     */
    bool is_using_lexbor() const { return use_lexbor_; }

    /**
     * @brief Enable or disable Lexbor parser
     * @param enable true to use Lexbor, false to use legacy parser
     */
    void set_use_lexbor(bool enable) { use_lexbor_ = enable; }

private:
    std::vector<StyleRule> rules_;
    
    // Lexbor components (for CSS3 support)
    std::unique_ptr<lexbor::LexborCSSParser> lexbor_parser_;
    std::unique_ptr<lexbor::LexborSelectorMatcher> lexbor_matcher_;
    std::unique_ptr<lexbor::LexborStyleComputer> lexbor_computer_;
    
    // Flag to control which parser to use
    bool use_lexbor_;

    /**
     * @brief Check if a selector matches the given shape (legacy)
     */
    bool matches_selector(
        const std::string& selector,
        const std::string& shape_id,
        const std::string& shape_type,
        const std::vector<std::string>& classes,
        const std::set<std::string>& pseudo_states) const;

    /**
     * @brief Calculate CSS specificity for a selector (legacy)
     * @param selector CSS selector
     * @return Specificity value (higher = more specific)
     */
    int calculate_specificity(const std::string& selector) const;

    /**
     * @brief Check if a property is inheritable from parent
     */
    bool is_inheritable(const std::string& property) const;

    /**
     * @brief Get default style for a shape type
     */
    std::map<std::string, std::string> get_default_styles(const std::string& shape_type) const;
};

} // namespace ddf
} // namespace whiteboard

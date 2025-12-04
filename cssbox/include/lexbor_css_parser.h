#pragma once

#include "lexbor_wrapper.h"
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include <set>
#include <memory>
#include <functional>
#include <optional>

// Forward declarations
struct KeyframeAnimation;

namespace cssbox {
    struct ComputedStyle;  // NEW: Typed property system
}

namespace cssbox {
namespace lexbor {

/**
 * @brief CSS Parser using Lexbor
 *
 * Replaces the manual CSS parser with Lexbor for full CSS3 support.
 */
class LexborCSSParser {
public:
    LexborCSSParser() = default;
    ~LexborCSSParser() = default;
    
    /**
     * @brief Parse CSS string
     * @param css CSS text to parse
     * @return true if parsing succeeded
     */
    bool parse(const std::string& css);
    
    /**
     * @brief Get parsed stylesheet
     * @return Pointer to stylesheet (owned by this parser)
     */
    lxb_css_stylesheet_t* get_stylesheet() const {
        return stylesheet_ ? stylesheet_->get() : nullptr;
    }
    
    /**
     * @brief Check if parser has errors
     */
    bool has_errors() const { return !errors_.empty(); }
    
    /**
     * @brief Get parsing errors
     */
    const std::vector<std::string>& get_errors() const { return errors_; }
    
    /**
     * @brief Clear parsed stylesheet and errors
     */
    void clear();
    
    /**
     * @brief Result of calc() evaluation
     */
    struct CalcResult {
        float value;
        std::string unit;
    };
    
    /**
     * @brief Evaluate calc() expression
     * @param expr calc() expression (e.g., "calc(100% - 20px)")
     * @param context_value Context value for percentage calculations (e.g., parent width)
     * @return Evaluated result or nullopt if invalid
     */
    std::optional<CalcResult> evaluate_calc(const std::string& expr, float context_value = 0.0f) const;
    
private:
    std::unique_ptr<CSSParser> parser_;
    std::unique_ptr<CSSStyleSheet> stylesheet_;
    std::vector<std::string> errors_;
    
    // Calc evaluation helpers
    struct Token {
        enum Type { NUMBER, OPERATOR, LPAREN, RPAREN, END };
        Type type;
        float value;
        std::string unit;
        char op;
    };
    
    std::vector<Token> tokenize_calc(const std::string& expr) const;
    std::optional<CalcResult> evaluate_tokens(const std::vector<Token>& tokens, float context_value) const;
    std::optional<CalcResult> parse_expression(const std::vector<Token>& tokens, size_t& pos, float context_value) const;
    std::optional<CalcResult> parse_term(const std::vector<Token>& tokens, size_t& pos, float context_value) const;
    std::optional<CalcResult> parse_factor(const std::vector<Token>& tokens, size_t& pos, float context_value) const;
};

/**
 * @brief CSS Rule structure (used by EnhancedStyleSheet)
 */
struct CSSRule {
    std::string selector;
    std::map<std::string, std::string> properties;
    int specificity;
    std::string media_query;  // e.g., "(min-width: 768px)" or empty for no media query
};

/**
 * @brief CSS Selector Matcher using Lexbor
 *
 * Matches CSS selectors against shapes with full CSS3 support.
 */
class LexborSelectorMatcher {
public:
    /**
     * @brief Check if selector matches shape
     * @param selector CSS selector string
     * @param shape_id Shape ID
     * @param shape_type Shape type (e.g., "rect", "circle")
     * @param classes CSS classes
     * @param attributes Shape attributes
     * @param pseudo_states Pseudo-states (e.g., "hover", "selected")
     * @return true if selector matches
     */
    bool matches(const std::string& selector,
                 const std::string& shape_id,
                 const std::string& shape_type,
                 const std::vector<std::string>& classes,
                 const std::map<std::string, std::string>& attributes,
                 const std::set<std::string>& pseudo_states) const;
    
    /**
     * @brief Check if selector matches shape (with shape hierarchy for combinators)
     * @param selector CSS selector string
     * @param shape_id Shape ID
     * @param shape_type Shape type
     * @param classes CSS classes
     * @param attributes Shape attributes
     * @param pseudo_states Pseudo-states
     * @param parent_shape_id Parent shape ID (for combinators)
     * @param get_shape_func Function to get shape by ID
     * @return true if selector matches
     */
    bool matches_with_hierarchy(
        const std::string& selector,
        const std::string& shape_id,
        const std::string& shape_type,
        const std::vector<std::string>& classes,
        const std::map<std::string, std::string>& attributes,
        const std::set<std::string>& pseudo_states,
        const std::string& parent_shape_id,
        std::function<const void*(const std::string&)> get_shape_func) const;
    
    /**
     * @brief Calculate CSS specificity for a selector
     * @param selector CSS selector string
     * @return Specificity value (higher = more specific)
     */
    int calculate_specificity(const std::string& selector) const;
    
private:
    /**
     * @brief Parse selector string into components
     */
    struct SelectorComponents {
        std::string type;           // Type selector (e.g., "rect")
        std::string id;             // ID selector (e.g., "shape1")
        std::vector<std::string> classes;  // Class selectors
        std::map<std::string, std::string> attributes;  // Attribute selectors
        std::vector<std::string> pseudo_classes;  // Pseudo-classes
        char combinator = '\0';     // Combinator (>, +, ~, or space)
    };
    
    /**
     * @brief Represents a complex selector with combinators
     * Example: ".parent > .child" becomes [{parent}, >, {child}]
     */
    struct ComplexSelector {
        std::vector<SelectorComponents> components;
        std::vector<char> combinators;  // Combinators between components
    };
    
    SelectorComponents parse_selector(const std::string& selector) const;
    ComplexSelector parse_complex_selector(const std::string& selector) const;
    
    bool matches_simple_selector(const SelectorComponents& sel,
                                 const std::string& shape_id,
                                 const std::string& shape_type,
                                 const std::vector<std::string>& classes,
                                 const std::map<std::string, std::string>& attributes,
                                 const std::set<std::string>& pseudo_states) const;
};

/**
 * @brief CSS Style Computer using Lexbor
 * 
 * Computes final styles with cascade, specificity, and inheritance.
 */
class LexborStyleComputer {
public:
    /**
     * @brief Compute final style for a shape
     * @param stylesheet Parsed CSS stylesheet
     * @param rules Vector of CSS rules to apply
     * @param shape_id Shape ID
     * @param shape_type Shape type
     * @param classes CSS classes
     * @param attributes Shape attributes
     * @param pseudo_states Pseudo-states
     * @param inline_style Inline styles (highest priority)
     * @param parent_style Parent's computed style (for inheritance)
     * @return Computed style map
     */
    std::map<std::string, std::string> compute_style(
        lxb_css_stylesheet_t* stylesheet,
        const std::vector<CSSRule>& rules,
        const std::string& shape_id,
        const std::string& shape_type,
        const std::vector<std::string>& classes,
        const std::map<std::string, std::string>& attributes,
        const std::set<std::string>& pseudo_states,
        const std::map<std::string, std::string>& inline_style,
        const std::map<std::string, std::string>& parent_style = {}) const;
    
private:
    LexborSelectorMatcher matcher_;
    
    /**
     * @brief Check if property is inheritable
     */
    bool is_inheritable(const std::string& property) const;
    
    /**
     * @brief Get default style for shape type
     */
    std::map<std::string, std::string> get_default_styles(const std::string& shape_type) const;
    
    /**
     * @brief Apply inheritance from parent
     */
    void apply_inheritance(std::map<std::string, std::string>& result,
                          const std::map<std::string, std::string>& parent_style) const;
};

/**
 * @brief CSS Variable Resolver
 * 
 * Resolves CSS custom properties (variables) with support for fallbacks and scoping.
 */
class CSSVariableResolver {
public:
    CSSVariableResolver() = default;
    ~CSSVariableResolver() = default;
    
    /**
     * @brief Set a CSS variable
     * @param name Variable name (e.g., "--primary-color")
     * @param value Variable value
     */
    void set_variable(const std::string& name, const std::string& value);
    
    /**
     * @brief Remove a CSS variable
     * @param name Variable name
     */
    void remove_variable(const std::string& name);
    
    /**
     * @brief Check if variable exists
     * @param name Variable name
     * @return true if variable is defined
     */
    bool has_variable(const std::string& name) const;
    
    /**
     * @brief Get variable value
     * @param name Variable name
     * @return Variable value or empty string if not found
     */
    std::string get_variable(const std::string& name) const;
    
    /**
     * @brief Resolve var() expressions in a string
     * @param value String containing var() expressions
     * @return String with var() expressions resolved
     */
    std::string resolve(const std::string& value) const;
    
    /**
     * @brief Create a scoped resolver (inherits from this one)
     * @return New resolver with this as parent
     */
    CSSVariableResolver create_scope() const;
    
    /**
     * @brief Clear all variables
     */
    void clear();
    
    /**
     * @brief Get all variables
     * @return Map of variable names to values
     */
    const std::map<std::string, std::string>& get_all_variables() const { return variables_; }
    
private:
    std::map<std::string, std::string> variables_;
    const CSSVariableResolver* parent_ = nullptr;
    
    /**
     * @brief Constructor for scoped resolver
     */
    CSSVariableResolver(const CSSVariableResolver* parent) : parent_(parent) {}
    
    /**
     * @brief Resolve a single var() expression
     * @param expr var() expression (e.g., "var(--color, blue)")
     * @param resolving_vars Set of variable names currently being resolved (for cycle detection)
     * @return Resolved value
     */
    std::string resolve_var(const std::string& expr, std::set<std::string>& resolving_vars) const;
    
    /**
     * @brief Recursive helper for resolve() with cycle detection
     * @param value String containing var() expressions
     * @param resolving_vars Set of variable names currently being resolved (for cycle detection)
     * @return String with var() expressions resolved
     */
    std::string resolve_recursive(const std::string& value, std::set<std::string>& resolving_vars) const;

    /**
     * @brief Parse var() expression to extract variable name and fallback
     * @param expr var() expression
     * @param var_name Output: variable name
     * @param fallback Output: fallback value
     * @return true if parsing succeeded
     */
    bool parse_var_expression(const std::string& expr, std::string& var_name, std::string& fallback) const;
};

/**
 * @brief Enhanced StyleSheet with Lexbor and caching
 * 
 * Drop-in replacement for the manual StyleSheet with better performance.
 */
class EnhancedStyleSheet {
public:
    EnhancedStyleSheet() = default;
    ~EnhancedStyleSheet() = default;
    
    /**
     * @brief Parse CSS string
     * @param css CSS text to parse
     * @return true if parsing succeeded
     */
    bool parse_css(const std::string& css);
    
    /**
     * @brief Add a CSS rule
     * @param selector CSS selector
     * @param properties Style properties
     */
    void add_rule(const std::string& selector,
                  const std::map<std::string, std::string>& properties);
    
    /**
     * @brief Compute style for a shape (with caching)
     * @param shape_id Shape ID
     * @param shape_type Shape type
     * @param classes CSS classes
     * @param attributes Shape attributes
     * @param pseudo_states Pseudo-states
     * @param inline_style Inline styles
     * @param parent_style Parent's computed style
     * @param child_index Child index for structural pseudo-classes
     * @param total_siblings Total number of siblings for structural pseudo-classes
     * @return Computed style map
     */
    std::map<std::string, std::string> compute_style(
        const std::string& shape_id,
        const std::string& shape_type,
        const std::vector<std::string>& classes,
        const std::map<std::string, std::string>& attributes,
        const std::set<std::string>& pseudo_states,
        const std::map<std::string, std::string>& inline_style,
        const std::map<std::string, std::string>& parent_style = {},
        int child_index = 0,
        int total_siblings = 1);

    /**
     * @brief Compute TYPED style for a shape (NEW: 60fps refactor)
     *
     * This is the new API that returns typed properties instead of strings.
     * Uses the same caching as compute_style() but returns cssbox::ComputedStyle.
     *
     * @param shape_id Shape ID
     * @param shape_type Shape type
     * @param classes CSS classes
     * @param attributes Shape attributes
     * @param pseudo_states Pseudo-states
     * @param inline_style Inline styles (still string-based for now)
     * @param parent_style Parent's computed style (for inheritance)
     * @param child_index Child index for structural pseudo-classes
     * @param total_siblings Total number of siblings
     * @return Typed computed style (zero runtime parsing!)
     */
    cssbox::ComputedStyle compute_style_typed(
        const std::string& shape_id,
        const std::string& shape_type,
        const std::vector<std::string>& classes,
        const std::map<std::string, std::string>& attributes,
        const std::set<std::string>& pseudo_states,
        const std::map<std::string, std::string>& inline_style,
        const cssbox::ComputedStyle* parent_style = nullptr,
        int child_index = 0,
        int total_siblings = 1);
    
    /**
     * @brief Clear cache
     */
    void clear_cache();
    
    /**
     * @brief Get cache statistics
     */
    struct CacheStats {
        size_t hits = 0;
        size_t misses = 0;
        size_t size = 0;
        
        float hit_rate() const {
            size_t total = hits + misses;
            return total > 0 ? static_cast<float>(hits) / total : 0.0f;
        }
    };
    
    CacheStats get_cache_stats() const { return cache_stats_; }
    
    /**
     * @brief Check if parser has errors
     */
    bool has_errors() const { return parser_.has_errors(); }
    
    /**
     * @brief Get parsing errors
     */
    const std::vector<std::string>& get_errors() const { return parser_.get_errors(); }
    
    /**
     * @brief Get variable resolver
     * @return Reference to the CSS variable resolver
     */
    CSSVariableResolver& get_variable_resolver() { return variable_resolver_; }
    const CSSVariableResolver& get_variable_resolver() const { return variable_resolver_; }

    /**
     * @brief Set CSS variable (convenience wrapper)
     * @param name Variable name (e.g., "--primary-color")
     * @param value Variable value
     */
    void set_variable(const std::string& name, const std::string& value) {
        variable_resolver_.set_variable(name, value);
        // Clear cache because all cached styles with var() references are now invalid
        clear_cache();
    }

    /**
     * @brief Add keyframe animation
     * @param animation Keyframe animation definition
     */
    void add_keyframe_animation(const KeyframeAnimation& animation);

    /**
     * @brief Get keyframe animation by name
     * @param name Animation name
     * @return Pointer to animation or nullptr if not found
     */
    const KeyframeAnimation* get_keyframe_animation(const std::string& name) const;

    /**
     * @brief Get all CSS rules
     * @return Vector of CSS rules
     */
    const std::vector<CSSRule>& get_rules() const { return rules_; }

    /**
     * @brief Set viewport dimensions for @media queries
     * @param width Viewport width in pixels
     * @param height Viewport height in pixels
     * @return true if viewport changed (requiring style recomputation)
     */
    bool set_viewport(float width, float height) {
        bool changed = (viewport_width_ != width || viewport_height_ != height);
        viewport_width_ = width;
        viewport_height_ = height;
        if (changed) {
            clear_cache();  // Media queries may now match differently
        }
        return changed;
    }

    /**
     * @brief Evaluate if a media query matches current viewport
     * @param media_query Media query string (e.g., "(min-width: 768px)")
     * @return true if query matches or is empty
     */
    bool evaluate_media_query(const std::string& media_query) const;

private:
    LexborCSSParser parser_;
    LexborStyleComputer computer_;
    LexborSelectorMatcher matcher_;
    CSSVariableResolver variable_resolver_;
    std::map<std::string, KeyframeAnimation> keyframe_animations_;

    // Manual rule storage (since Lexbor doesn't expose rules)
    std::vector<CSSRule> rules_;

    // Cache: cache_key -> computed_style
    std::unordered_map<std::string, std::map<std::string, std::string>> cache_;
    mutable CacheStats cache_stats_;

    // Viewport dimensions for @media queries
    float viewport_width_ = 800.0f;
    float viewport_height_ = 600.0f;

    static constexpr size_t MAX_CACHE_SIZE = 200;  // Reduced from 1000
    
    /**
     * @brief Generate cache key
     */
    std::string generate_cache_key(
        const std::string& shape_id,
        const std::string& shape_type,
        const std::vector<std::string>& classes,
        const std::set<std::string>& pseudo_states,
        const std::map<std::string, std::string>& attributes,
        const std::map<std::string, std::string>& inline_style) const;
    
    /**
     * @brief Evict LRU cache entry
     */
    void evict_lru();
};

} // namespace lexbor
} // namespace whiteboard

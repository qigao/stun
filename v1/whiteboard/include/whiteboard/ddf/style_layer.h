#pragma once

#include <string>
#include <map>
#include <memory>
#include <vector>
#include <set>
#include <whiteboard/ddf/stylesheet.h>

namespace whiteboard {
namespace ddf {

// Forward declarations
struct Shape;

/**
 * @brief Manages CSS-like stylesheets and computed styles for DDF shapes
 * 
 * The StyleLayer provides:
 * - Multiple stylesheet management
 * - Style computation with cascade logic
 * - Pseudo-state management (hover, selected, active, etc.)
 * - Computed style caching for performance
 */
class StyleLayer {
public:
    StyleLayer() = default;
    ~StyleLayer() = default;

    // Stylesheet management
    
    /**
     * @brief Add a stylesheet to the layer
     * @param id Unique identifier for the stylesheet
     * @param stylesheet The stylesheet to add
     */
    void add_stylesheet(const std::string& id, std::unique_ptr<StyleSheet> stylesheet);
    
    /**
     * @brief Remove a stylesheet from the layer
     * @param id Stylesheet identifier
     */
    void remove_stylesheet(const std::string& id);
    
    /**
     * @brief Get a stylesheet by ID
     * @param id Stylesheet identifier
     * @return Pointer to stylesheet or nullptr if not found
     */
    StyleSheet* get_stylesheet(const std::string& id);
    
    /**
     * @brief Get all stylesheets
     */
    const std::map<std::string, std::unique_ptr<StyleSheet>>& get_stylesheets() const {
        return stylesheets_;
    }
    
    // Style computation
    
    /**
     * @brief Compute final style for a shape using all stylesheets
     * @param shape The shape to compute styles for
     * @param parent_style Parent's computed style (for inheritance)
     * @return Computed style map
     */
    std::map<std::string, std::string> compute_style_for_shape(
        const Shape& shape,
        const std::map<std::string, std::string>& parent_style = {}) const;
    
    // Pseudo-state management
    
    /**
     * @brief Update a pseudo-state for a shape
     * @param shape_id Shape identifier
     * @param state Pseudo-state name (e.g., "hover", "selected", "active")
     * @param active Whether the state is active or not
     */
    void update_pseudo_state(const std::string& shape_id, const std::string& state, bool active);
    
    /**
     * @brief Get all active pseudo-states for a shape
     * @param shape_id Shape identifier
     * @return Set of active pseudo-state names
     */
    std::set<std::string> get_pseudo_states(const std::string& shape_id) const;
    
    /**
     * @brief Clear all pseudo-states for a shape
     * @param shape_id Shape identifier
     */
    void clear_pseudo_states(const std::string& shape_id);
    
    /**
     * @brief Clear all pseudo-states for all shapes
     */
    void clear_all_pseudo_states();
    
    // Cache management
    
    /**
     * @brief Invalidate computed style cache
     * Call this when stylesheets are modified
     */
    void invalidate_computed_styles();
    
    /**
     * @brief Invalidate computed style cache for a specific shape
     * @param shape_id Shape identifier
     */
    void invalidate_computed_style(const std::string& shape_id);

private:
    // Stylesheets indexed by ID
    std::map<std::string, std::unique_ptr<StyleSheet>> stylesheets_;
    
    // Pseudo-states for each shape
    std::map<std::string, std::set<std::string>> pseudo_states_;
    
    // Computed style cache (shape_id -> computed_style)
    mutable std::map<std::string, std::map<std::string, std::string>> computed_style_cache_;
};

} // namespace ddf
} // namespace whiteboard

#include <whiteboard/ddf/style_layer.h>
#include <whiteboard/ddf/stylesheet.h>
#include <whiteboard/ddf/shape_layer.h>
#include <algorithm>

namespace whiteboard {
namespace ddf {

void StyleLayer::add_stylesheet(const std::string& id, std::unique_ptr<StyleSheet> stylesheet) {
    if (!stylesheet) return;
    
    stylesheets_[id] = std::move(stylesheet);
    
    // Invalidate cache since we added a new stylesheet
    invalidate_computed_styles();
}

void StyleLayer::remove_stylesheet(const std::string& id) {
    auto it = stylesheets_.find(id);
    if (it != stylesheets_.end()) {
        stylesheets_.erase(it);
        
        // Invalidate cache since we removed a stylesheet
        invalidate_computed_styles();
    }
}

StyleSheet* StyleLayer::get_stylesheet(const std::string& id) {
    auto it = stylesheets_.find(id);
    if (it != stylesheets_.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::map<std::string, std::string> StyleLayer::compute_style_for_shape(
    const Shape& shape,
    const std::map<std::string, std::string>& parent_style) const {
    
    // Check cache first
    auto cache_it = computed_style_cache_.find(shape.id);
    if (cache_it != computed_style_cache_.end()) {
        return cache_it->second;
    }
    
    // Get pseudo-states for this shape
    std::set<std::string> pseudo_states;
    auto ps_it = pseudo_states_.find(shape.id);
    if (ps_it != pseudo_states_.end()) {
        pseudo_states = ps_it->second;
    }
    
    // Start with empty computed style
    std::map<std::string, std::string> computed_style;
    
    // Use each stylesheet's compute_style method (which uses Lexbor if available)
    // This allows stylesheets to use their internal Lexbor parser
    for (const auto& [id, stylesheet] : stylesheets_) {
        auto sheet_style = stylesheet->compute_style(
            shape.id,
            shape.type,
            shape.classes,
            pseudo_states,
            shape.inline_style,
            parent_style
        );
        
        // Merge styles from this stylesheet
        for (const auto& [key, value] : sheet_style) {
            computed_style[key] = value;
        }
    }
    
    // Cache the result
    computed_style_cache_[shape.id] = computed_style;
    
    return computed_style;
}

void StyleLayer::update_pseudo_state(const std::string& shape_id, const std::string& state, bool active) {
    if (active) {
        // Add the pseudo-state
        pseudo_states_[shape_id].insert(state);
    } else {
        // Remove the pseudo-state
        auto it = pseudo_states_.find(shape_id);
        if (it != pseudo_states_.end()) {
            it->second.erase(state);
            
            // Clean up empty sets
            if (it->second.empty()) {
                pseudo_states_.erase(it);
            }
        }
    }
    
    // Invalidate cache for this shape since pseudo-states changed
    invalidate_computed_style(shape_id);
}

std::set<std::string> StyleLayer::get_pseudo_states(const std::string& shape_id) const {
    auto it = pseudo_states_.find(shape_id);
    if (it != pseudo_states_.end()) {
        return it->second;
    }
    return {};
}

void StyleLayer::clear_pseudo_states(const std::string& shape_id) {
    auto it = pseudo_states_.find(shape_id);
    if (it != pseudo_states_.end()) {
        pseudo_states_.erase(it);
        invalidate_computed_style(shape_id);
    }
}

void StyleLayer::clear_all_pseudo_states() {
    pseudo_states_.clear();
    invalidate_computed_styles();
}

void StyleLayer::invalidate_computed_styles() {
    computed_style_cache_.clear();
}

void StyleLayer::invalidate_computed_style(const std::string& shape_id) {
    auto it = computed_style_cache_.find(shape_id);
    if (it != computed_style_cache_.end()) {
        computed_style_cache_.erase(it);
    }
}

} // namespace ddf
} // namespace whiteboard

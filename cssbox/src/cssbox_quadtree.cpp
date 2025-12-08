/*
 * NanoVG CSS - Quadtree Layout Engine Implementation
 */

#include "cssbox_internal.h"
#include "cssbox_quadtree.h"
#include <fmtlog.h>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <sstream>
#include <vector>
#include <cstring>
#include <string_view>
#include <unordered_map>

// Windows macro cleanup (wingdi.h defines RELATIVE/ABSOLUTE)
#ifdef RELATIVE
#undef RELATIVE
#endif
#ifdef ABSOLUTE
#undef ABSOLUTE
#endif

namespace cssbox {

// ============================================================================
// Performance Caches
// ============================================================================

// Font resolution cache: Maps (font_family hash, weight) -> resolved font name
// Avoids repeated string comparisons in tight layout loops
namespace {
    struct FontCacheKey {
        size_t family_hash;
        int weight;
        
        bool operator==(const FontCacheKey& other) const {
            return family_hash == other.family_hash && weight == other.weight;
        }
    };
    
    struct FontCacheKeyHash {
        size_t operator()(const FontCacheKey& k) const {
            return k.family_hash ^ (static_cast<size_t>(k.weight) << 16);
        }
    };
    
    static std::unordered_map<FontCacheKey, std::string, FontCacheKeyHash> g_font_cache;
    static std::hash<std::string> g_string_hasher;
    
    // Get resolved font face with caching
    inline const std::string& get_resolved_font(const std::string& family, FontWeight weight) {
        FontCacheKey key{g_string_hasher(family), static_cast<int>(weight)};
        
        auto it = g_font_cache.find(key);
        if (it != g_font_cache.end()) {
            return it->second;
        }
        
        // Resolve font face
        std::string resolved = family;
        if (weight >= FontWeight::BOLD && family == "sans-serif") {
            resolved = "sans-serif-Bold";
        }
        // Add more font mappings here as needed
        
        auto [inserted_it, _] = g_font_cache.emplace(key, std::move(resolved));
        return inserted_it->second;
    }
    
    // Text measurement cache: Avoids expensive nvgTextBounds calls
    struct TextMeasureKey {
        size_t text_hash;
        float font_size;
        size_t font_hash;
        
        bool operator==(const TextMeasureKey& other) const {
            return text_hash == other.text_hash && 
                   font_size == other.font_size && 
                   font_hash == other.font_hash;
        }
    };
    
    struct TextMeasureKeyHash {
        size_t operator()(const TextMeasureKey& k) const {
            size_t h = k.text_hash;
            h ^= std::hash<float>{}(k.font_size) << 1;
            h ^= k.font_hash << 2;
            return h;
        }
    };
    
    struct TextMeasureResult {
        float width;
        float height;
    };
    
    static std::unordered_map<TextMeasureKey, TextMeasureResult, TextMeasureKeyHash> g_text_measure_cache;
    static constexpr size_t TEXT_CACHE_MAX_SIZE = 4096;  // Limit cache size
    
    // Get text bounds with caching
    inline TextMeasureResult get_text_bounds_cached(
        NVGcontext* vg,
        const std::string& text,
        float font_size,
        const std::string& font_face
    ) {
        TextMeasureKey key{
            g_string_hasher(text),
            font_size,
            g_string_hasher(font_face)
        };
        
        auto it = g_text_measure_cache.find(key);
        if (it != g_text_measure_cache.end()) {
            return it->second;
        }
        
        // Cache miss - measure text
        float bounds[4];
        nvgTextBounds(vg, 0, 0, text.c_str(), nullptr, bounds);
        
        TextMeasureResult result{bounds[2] - bounds[0], bounds[3] - bounds[1]};
        
        // Evict old entries if cache is too large
        if (g_text_measure_cache.size() >= TEXT_CACHE_MAX_SIZE) {
            g_text_measure_cache.clear();  // Simple eviction strategy
        }
        
        g_text_measure_cache.emplace(key, result);
        return result;
    }
    
    // Measure single word using string_view (zero-copy)
    inline float measure_word_width(
        NVGcontext* vg,
        std::string_view word,
        float font_size,
        const std::string& font_face
    ) {
        // For single words, use direct measurement (caching overhead not worth it)
        float bounds[4];
        // nvgTextBounds needs null-terminated string, but word is a view
        // Use a small static buffer for common case
        thread_local char word_buffer[256];
        size_t len = std::min(word.size(), sizeof(word_buffer) - 1);
        std::memcpy(word_buffer, word.data(), len);
        word_buffer[len] = '\0';
        nvgTextBounds(vg, 0, 0, word_buffer, nullptr, bounds);
        return bounds[2] - bounds[0];
    }
} // anonymous namespace

// Helper to get string property from computed style
// FIXED: Skip compute_style if inline_style is empty (e.g., when using typed properties directly)
static std::string get_string_property(cssboxRenderer* renderer, cssboxElement* element, const std::string& prop) {
    if (!renderer || !element) return "";

    // If inline_style is empty, typed properties are being used directly - no CSS parsing needed
    if (element->inline_style.empty()) return "";

    auto style = renderer->stylesheet->compute_style(
        element->id, element->type, element->classes, element->attributes,
        element->pseudo_states, element->inline_style, {},
        element->child_index, element->total_siblings
    );
    auto it = style.find(prop);
    if (it != style.end()) return it->second;
    return "";
}

struct GridArea {
    int row_start, col_start;
    int row_end, col_end;
};

// ============================================================================
// Constructor / Destructor
// ============================================================================

QuadtreeLayoutEngine::QuadtreeLayoutEngine(float viewport_width, float viewport_height)
    : viewport_width_(viewport_width)
    , viewport_height_(viewport_height) {
}

QuadtreeLayoutEngine::~QuadtreeLayoutEngine() {
}

void QuadtreeLayoutEngine::set_viewport(float width, float height) {
    viewport_width_ = width;
    viewport_height_ = height;
}

// ============================================================================
// Tree Building
// ============================================================================

LayoutNode* QuadtreeLayoutEngine::build_tree(cssboxElement* root, cssboxRenderer* renderer) {
    if (!root) return nullptr;
    
    LayoutNode* node = new LayoutNode(root);
    
    // Determine layout type from element's display property
    if (root->style.display == Display::FLEX) {
        node->type = LayoutNode::FLEX;
    } else if (root->style.display == Display::GRID) {
        node->type = LayoutNode::GRID;
    } else {
        node->type = LayoutNode::BLOCK;
    }
    
    // Copy CSS constraints from element
    node->constraints.width = root->style.width;
    node->constraints.height = root->style.height;
    node->constraints.min_width = root->style.min_width;
    node->constraints.max_width = root->style.max_width;
    node->constraints.min_height = root->style.min_height;
    node->constraints.max_height = root->style.max_height;
    
    // Copy padding, margin, border
    for (int i = 0; i < 4; i++) {
        node->constraints.padding[i] = root->style.padding[i].resolve(0, 16, viewport_width_);
        node->constraints.margin[i] = root->style.margin[i].resolve(0, 16, viewport_width_);
        node->constraints.border[i] = root->style.border.width[i];
    }

    // Parse grid-column and grid-row from computed CSS (if not already set in typed style)
    // FIXED: Only call compute_style if inline_style is not empty (to avoid crashes when renderer has no CSS)
    std::map<std::string, std::string> computed_style;
    if (!root->inline_style.empty()) {
        computed_style = renderer->stylesheet->compute_style(
            root->id, root->type, root->classes, root->attributes,
            root->pseudo_states, root->inline_style, {},
            root->child_index, root->total_siblings
        );
    }

    // Helper lambda to check if a string is a number
    auto is_number = [](const std::string& s) -> bool {
        if (s.empty()) return false;
        size_t start = (s[0] == '-' || s[0] == '+') ? 1 : 0;
        if (start >= s.size()) return false;
        for (size_t i = start; i < s.size(); ++i) {
            if (!std::isdigit(s[i])) return false;
        }
        return true;
    };

    auto grid_col_it = computed_style.find("grid-column");
    if (grid_col_it != computed_style.end() && root->style.grid_column.start <= 0 &&
        root->style.grid_column.start_name.empty()) {
        // Parse "1 / 4", "sidebar-start / content-end", or "2 / span 2" format
        std::string val = grid_col_it->second;
        size_t slash_pos = val.find('/');
        if (slash_pos != std::string::npos) {
            std::string start_str = val.substr(0, slash_pos);
            std::string end_str = val.substr(slash_pos + 1);

            // Trim whitespace
            start_str.erase(0, start_str.find_first_not_of(" \t"));
            start_str.erase(start_str.find_last_not_of(" \t") + 1);
            end_str.erase(0, end_str.find_first_not_of(" \t"));
            end_str.erase(end_str.find_last_not_of(" \t") + 1);

            // Check if start is a number or a named line
            if (is_number(start_str)) {
                root->style.grid_column.start = std::atoi(start_str.c_str());
            } else {
                root->style.grid_column.start_name = start_str;
            }

            if (end_str.find("span") != std::string::npos) {
                size_t span_pos = end_str.find("span");
                std::string span_str = end_str.substr(span_pos + 4);
                span_str.erase(0, span_str.find_first_not_of(" \t"));
                root->style.grid_column.span = std::atoi(span_str.c_str());
            } else if (is_number(end_str)) {
                root->style.grid_column.end = std::atoi(end_str.c_str());
            } else {
                root->style.grid_column.end_name = end_str;
            }
        } else {
            // Single value
            if (is_number(val)) {
                root->style.grid_column.start = std::atoi(val.c_str());
            } else {
                root->style.grid_column.start_name = val;
            }
        }
    }

    auto grid_row_it = computed_style.find("grid-row");
    if (grid_row_it != computed_style.end() && root->style.grid_row.start <= 0 &&
        root->style.grid_row.start_name.empty()) {
        std::string val = grid_row_it->second;
        size_t slash_pos = val.find('/');
        if (slash_pos != std::string::npos) {
            std::string start_str = val.substr(0, slash_pos);
            std::string end_str = val.substr(slash_pos + 1);

            start_str.erase(0, start_str.find_first_not_of(" \t"));
            start_str.erase(start_str.find_last_not_of(" \t") + 1);
            end_str.erase(0, end_str.find_first_not_of(" \t"));
            end_str.erase(end_str.find_last_not_of(" \t") + 1);

            if (is_number(start_str)) {
                root->style.grid_row.start = std::atoi(start_str.c_str());
            } else {
                root->style.grid_row.start_name = start_str;
            }

            if (end_str.find("span") != std::string::npos) {
                size_t span_pos = end_str.find("span");
                std::string span_str = end_str.substr(span_pos + 4);
                span_str.erase(0, span_str.find_first_not_of(" \t"));
                root->style.grid_row.span = std::atoi(span_str.c_str());
            } else if (is_number(end_str)) {
                root->style.grid_row.end = std::atoi(end_str.c_str());
            } else {
                root->style.grid_row.end_name = end_str;
            }
        } else {
            if (is_number(val)) {
                root->style.grid_row.start = std::atoi(val.c_str());
            } else {
                root->style.grid_row.start_name = val;
            }
        }
    }
    
    // Recursively build children
    // IMPORTANT: Copy children locally before recursing, because cssboxGetChildren
    // uses a static cache that gets overwritten by recursive calls
    int child_count = 0;
    cssboxElement** children_ptr = cssboxGetChildren(renderer, root, &child_count);
    std::vector<cssboxElement*> children_copy(children_ptr, children_ptr + child_count);

    for (cssboxElement* child : children_copy) {
        // Skip invisible elements AND elements with display: none
        if (child->visible && child->style.display != Display::NONE) {
            LayoutNode* child_node = build_tree(child, renderer);
            if (child_node) {
                child_node->parent = node;
                node->children.push_back(child_node);
            }
        }
    }
    
    return node;
}

// ============================================================================
// Main Layout Algorithm
// ============================================================================

void QuadtreeLayoutEngine::compute_layout(LayoutNode* root, cssboxRenderer* renderer) {
    if (!root) return;
    
    // Phase 1: Collect constraints (top-down)
    LayoutConstraints root_constraints;
    root_constraints.available_width = viewport_width_;
    root_constraints.available_height = viewport_height_;
    collect_constraints(root, root_constraints);
    
    // Phase 2: Calculate dimensions (bottom-up)
    calculate_dimensions(root, renderer);
    
    // Phase 3: Calculate positions (top-down)
    calculate_positions(root, 0, 0);
}

// ============================================================================
// Phase 1: Constraint Collection (Top-Down)
// ============================================================================

void QuadtreeLayoutEngine::collect_constraints(
    LayoutNode* node,
    const LayoutConstraints& parent_constraints) {
    
    // Inherit available space from parent
    node->constraints.available_width = parent_constraints.available_width;
    node->constraints.available_height = parent_constraints.available_height;
    
    // Determine if this node restricts available space for children (fixed size)
    float effective_available_width = node->constraints.available_width;
    if (!node->constraints.width.is_auto()) {
        float fixed_w = resolve_length(node->constraints.width, parent_constraints.available_width, 16.0f);
        if (fixed_w > 0) effective_available_width = fixed_w;
    }

    float effective_available_height = node->constraints.available_height;
    if (!node->constraints.height.is_auto()) {
        float fixed_h = resolve_length(node->constraints.height, parent_constraints.available_height, 16.0f);
        if (fixed_h > 0) effective_available_height = fixed_h;
    }

    // Calculate available space for children (subtract padding and border)
    float child_available_width = effective_available_width
        - node->constraints.padding[1] - node->constraints.padding[3]  // right + left
        - node->constraints.border[1] - node->constraints.border[3];
    
    float child_available_height = effective_available_height
        - node->constraints.padding[0] - node->constraints.padding[2]  // top + bottom
        - node->constraints.border[0] - node->constraints.border[2];
    
    // Propagate to children
    LayoutConstraints child_constraints = node->constraints;
    child_constraints.available_width = std::max(0.0f, child_available_width);
    child_constraints.available_height = std::max(0.0f, child_available_height);
    
    for (auto* child : node->children) {
        collect_constraints(child, child_constraints);
    }
}

// ============================================================================
// Phase 2: Dimension Calculation (Bottom-Up)
// ============================================================================

void QuadtreeLayoutEngine::calculate_dimensions(LayoutNode* node, cssboxRenderer* renderer) {
    // Calculate children first (bottom-up)
    for (auto* child : node->children) {
        calculate_dimensions(child, renderer);
    }
    
    // Calculate this node's dimensions based on type
    switch (node->type) {
        case LayoutNode::FLEX:
            calculate_flex_dimensions(node, renderer);
            break;
        case LayoutNode::GRID:
            calculate_grid_dimensions(node, renderer);
            break;
        case LayoutNode::BLOCK:
        default:
            calculate_block_dimensions(node, renderer);
            break;
    }
    
    // Apply min/max constraints
    if (!node->constraints.min_width.is_auto()) {
        float min_w = resolve_length(node->constraints.min_width, viewport_width_, 16.0f);
        node->width = std::max(node->width, min_w);
    }
    // Apply min/max constraints (FIXED: max first, then min, so min wins if min > max)
    if (!node->constraints.max_width.is_auto()) {
        float max_w = resolve_length(node->constraints.max_width, viewport_width_, 16.0f);
        node->width = std::min(node->width, max_w);
    }
    if (!node->constraints.min_width.is_auto()) {
        float min_w = resolve_length(node->constraints.min_width, viewport_width_, 16.0f);
        node->width = std::max(node->width, min_w);
    }
    
    if (!node->constraints.max_height.is_auto()) {
        float max_h = resolve_length(node->constraints.max_height, viewport_height_, 16.0f);
        node->height = std::min(node->height, max_h);
    }
    if (!node->constraints.min_height.is_auto()) {
        float min_h = resolve_length(node->constraints.min_height, viewport_height_, 16.0f);
        node->height = std::max(node->height, min_h);
    }
    
    // Calculate content dimensions (subtract padding and border)
    node->content_width = node->width
        - node->constraints.padding[1] - node->constraints.padding[3]
        - node->constraints.border[1] - node->constraints.border[3];
    
    node->content_height = node->height
        - node->constraints.padding[0] - node->constraints.padding[2]
        - node->constraints.border[0] - node->constraints.border[2];
    
    node->content_width = std::max(0.0f, node->content_width);
    node->content_height = std::max(0.0f, node->content_height);
}

void QuadtreeLayoutEngine::calculate_block_dimensions(LayoutNode* node, cssboxRenderer* renderer) {
    // Determine if we need to shrink-to-fit:
    // 1. Absolute/fixed positioning with auto width
    // 2. Flex item with auto width (parent is flex container)
    bool is_absolute = (node->element->style.position == Position::ABSOLUTE ||
                       node->element->style.position == Position::FIXED);
    bool is_flex_item = node->parent && node->parent->type == LayoutNode::FLEX;
    bool shrink_to_fit = (is_absolute || is_flex_item) && node->constraints.width.is_auto();
    
    // Width: resolve against available space
    if (!node->constraints.width.is_auto()) {
        node->width = resolve_length(node->constraints.width, node->constraints.available_width, 16.0f);
        
        // Handle box-sizing: content-box (default) means width is content width, so add padding/border
        if (node->element->style.box_sizing == BoxSizing::CONTENT_BOX) {
            node->width += node->constraints.padding[1] + node->constraints.padding[3] +
                           node->constraints.border[1] + node->constraints.border[3];
        }
    } else if (shrink_to_fit) {
        // Initial width 0, will expand to text content
        node->width = 0; 
    } else {
        node->width = node->constraints.available_width;
    }
    
    float text_width = 0;
    float text_height = 0;
    bool has_text = !node->element->text_content.empty();

    // Measure text if needed
    if (has_text && (shrink_to_fit || node->constraints.height.is_auto())) {
        if (renderer && renderer->vg) {
            NVGcontext* vg = renderer->vg;
            
            // Setup font (use cached resolution)
            const std::string& font_face = get_resolved_font(
                node->element->style.font_family,
                node->element->style.font_weight
            );
            nvgFontFace(vg, font_face.c_str());
            nvgFontSize(vg, node->element->style.font_size);
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
            // nvgTextLineHeight(vg, ...); // Default is usually fine
            
            float bounds[4];
            if (shrink_to_fit) {
                // Measure single line text width (no wrapping constrained by 0 width)
                // If it was constrained by max-width, we should check.
                // For now assume single line if shrink-to-fit width.
                nvgTextBounds(vg, 0, 0, node->element->text_content.c_str(), nullptr, bounds);
                text_width = bounds[2] - bounds[0];
                text_height = bounds[3] - bounds[1];
            } else {
                // Width is fixed/fill, use TextBox for wrapping
                float wrap_width = node->width - node->constraints.padding[1] - node->constraints.padding[3] 
                                  - node->constraints.border[1] - node->constraints.border[3];
                if (wrap_width > 0) {
                    nvgTextBoxBounds(vg, 0, 0, wrap_width, node->element->text_content.c_str(), nullptr, bounds);
                    text_width = bounds[2] - bounds[0];
                    text_height = bounds[3] - bounds[1];
                } else {
                    // Fallback to text bounds if width is 0?
                    nvgTextBounds(vg, 0, 0, node->element->text_content.c_str(), nullptr, bounds);
                    text_width = bounds[2] - bounds[0];
                    text_height = bounds[3] - bounds[1];
                }
            }
        }
    }

    // Apply shrink-to-fit width
    if (shrink_to_fit) {
        float content_width = text_width;

        // For containers with children, calculate width from children
        if (!node->children.empty()) {
            float children_width = 0;
            for (auto* child : node->children) {
                if (child->element->style.position == Position::ABSOLUTE ||
                    child->element->style.position == Position::FIXED) continue;
                // Sum children widths (for block layout, take max; but this is shrink-to-fit)
                // For flex-row children, we'd sum. For block children, we'd take max.
                // Since this node itself is a flex item, assume its children are block-stacked
                children_width = std::max(children_width,
                    child->width + child->constraints.margin[1] + child->constraints.margin[3]);
            }
            content_width = std::max(content_width, children_width);
        }

        node->width = content_width + node->constraints.padding[1] + node->constraints.padding[3] +
                      node->constraints.border[1] + node->constraints.border[3];
    }
    
    // Height: resolve against available space or sum children
    if (!node->constraints.height.is_auto()) {
        node->height = resolve_length(node->constraints.height, node->constraints.available_height, 16.0f);
        
        // Handle box-sizing
        if (node->element->style.box_sizing == BoxSizing::CONTENT_BOX) {
            node->height += node->constraints.padding[0] + node->constraints.padding[2] +
                            node->constraints.border[0] + node->constraints.border[2];
        }
    } else {
        float total_height = 0;
        bool has_flow_children = false;
        
        for (auto* child : node->children) {
            // Skip absolute positioned elements from flow height calculation
            if (child->element->style.position == Position::ABSOLUTE || 
                child->element->style.position == Position::FIXED) continue;
                
            total_height += child->height + child->constraints.margin[0] + child->constraints.margin[2];
            has_flow_children = true;
        }
        
        // If no flow children and has text, add text height
        if (!has_flow_children && has_text) {
             total_height += text_height;
        }
        // TODO: Mixed children and text? Usually wrapped in anonymous blocks.
        // If we have flow children, text content is usually ignored in this simple engine unless it's a child node itself.
        // But NanoVG CSS might store text directly on the element.
        // If so, we should probably add text_height anyway?
        // But `text_height` calculated above might be full height.
        // For now, only add if no children.

        node->height = total_height + node->constraints.padding[0] + node->constraints.padding[2] +
                       node->constraints.border[0] + node->constraints.border[2];
    }
}

void QuadtreeLayoutEngine::calculate_flex_dimensions(LayoutNode* node, cssboxRenderer* renderer) {
    // Sort children by order
    if (!node->children.empty()) {
        std::stable_sort(node->children.begin(), node->children.end(), 
            [](const LayoutNode* a, const LayoutNode* b) {
                return a->element->style.order < b->element->style.order;
            });
    }

    // Container dimensions: resolve against available space
    // For flex containers that are also flex items with auto width, we use shrink-to-fit
    bool is_flex_item = node->parent && node->parent->type == LayoutNode::FLEX;
    bool shrink_to_fit = is_flex_item && node->constraints.width.is_auto();

    if (!node->constraints.width.is_auto()) {
        node->width = resolve_length(node->constraints.width, node->constraints.available_width, 16.0f);
        if (node->element->style.box_sizing == BoxSizing::CONTENT_BOX) {
            node->width += node->constraints.padding[1] + node->constraints.padding[3] +
                           node->constraints.border[1] + node->constraints.border[3];
        }
    } else {
        // For now, use available_width; will be recalculated from children if shrink_to_fit
        node->width = node->constraints.available_width;
    }
    
    if (!node->constraints.height.is_auto()) {
        node->height = resolve_length(node->constraints.height, node->constraints.available_height, 16.0f);
        if (node->element->style.box_sizing == BoxSizing::CONTENT_BOX) {
            node->height += node->constraints.padding[0] + node->constraints.padding[2] +
                            node->constraints.border[0] + node->constraints.border[2];
        }
    } else {
        node->height = node->constraints.available_height;
    }
    
    // Get flex properties
    bool is_row = (node->element->style.flex_direction == FlexDirection::ROW ||
                   node->element->style.flex_direction == FlexDirection::ROW_REVERSE);
    bool can_wrap = (node->element->style.flex_wrap != FlexWrap::NOWRAP);

    // Calculate available space for children (subtract padding)
    float available_main = is_row ?
        (node->width - node->constraints.padding[1] - node->constraints.padding[3]) :
        (node->height - node->constraints.padding[0] - node->constraints.padding[2]);

    float available_cross = is_row ?
        (node->height - node->constraints.padding[0] - node->constraints.padding[2]) :
        (node->width - node->constraints.padding[1] - node->constraints.padding[3]);

    // For shrink-to-fit containers, skip flex space distribution
    // Children keep their intrinsic sizes, container shrinks to fit
    if (!shrink_to_fit) {
        if (!can_wrap) {
            // Single-line flex (existing algorithm)
            calculate_flex_single_line(node, is_row, available_main, available_cross);
        } else {
            // Multi-line flex with wrapping
            calculate_flex_multi_line(node, is_row, available_main, available_cross);
        }
    }

    // BUGFIX: Recalculate container dimensions based on children if auto-sized
    bool width_was_auto = node->constraints.width.is_auto();
    bool height_was_auto = node->constraints.height.is_auto();

    if (width_was_auto || height_was_auto) {
        float content_main_size = 0;
        float content_cross_size = 0;
        float gap_value = resolve_length(node->element->style.gap, available_main, 16.0f);
        int flow_child_count = 0;

        if (is_row) {
            // Row direction: width is main axis, height is cross axis
            for (auto* child : node->children) {
                if (child->element->style.position == Position::ABSOLUTE ||
                    child->element->style.position == Position::FIXED) continue;
                content_main_size += child->width + child->constraints.margin[1] + child->constraints.margin[3];
                content_cross_size = std::max(content_cross_size,
                    child->height + child->constraints.margin[0] + child->constraints.margin[2]);
                flow_child_count++;
            }

            // Add gaps between children
            if (flow_child_count > 1) {
                content_main_size += gap_value * (flow_child_count - 1);
            }

            if (width_was_auto) {
                node->width = content_main_size + node->constraints.padding[1] + node->constraints.padding[3] +
                              node->constraints.border[1] + node->constraints.border[3];
            }
            if (height_was_auto) {
                node->height = content_cross_size + node->constraints.padding[0] + node->constraints.padding[2] +
                               node->constraints.border[0] + node->constraints.border[2];
            }
        } else {
            // Column direction: height is main axis, width is cross axis
            for (auto* child : node->children) {
                if (child->element->style.position == Position::ABSOLUTE ||
                    child->element->style.position == Position::FIXED) continue;
                content_main_size += child->height + child->constraints.margin[0] + child->constraints.margin[2];
                content_cross_size = std::max(content_cross_size,
                    child->width + child->constraints.margin[1] + child->constraints.margin[3]);
                flow_child_count++;
            }

            // Add gaps between children
            if (flow_child_count > 1) {
                content_main_size += gap_value * (flow_child_count - 1);
            }

            if (height_was_auto) {
                node->height = content_main_size + node->constraints.padding[0] + node->constraints.padding[2] +
                               node->constraints.border[0] + node->constraints.border[2];
            }
            if (width_was_auto) {
                node->width = content_cross_size + node->constraints.padding[1] + node->constraints.padding[3] +
                              node->constraints.border[1] + node->constraints.border[3];
            }
        }
    }
}

void QuadtreeLayoutEngine::calculate_flex_single_line(LayoutNode* node, bool is_row, float available_main, float available_cross) {
    // Calculate base sizes
    std::vector<float> base_sizes;
    float total_base = 0;
    float total_grow = 0;
    float total_shrink = 0;
    
    for (auto* child : node->children) {
        // Skip absolute positioned elements
        if (child->element->style.position == Position::ABSOLUTE || 
            child->element->style.position == Position::FIXED) {
            base_sizes.push_back(0); // Placeholder
            continue;
        }
        
        float base_size = 0;

        if (!child->element->style.flex_basis.is_auto()) {
            base_size = resolve_length(child->element->style.flex_basis, available_main, 16.0f);
        } else if (is_row && !child->constraints.width.is_auto()) {
            base_size = resolve_length(child->constraints.width, available_main, 16.0f);
        } else if (!is_row && !child->constraints.height.is_auto()) {
            base_size = resolve_length(child->constraints.height, available_main, 16.0f);
        } else {
            // flex-basis: auto AND main size: auto
            // Use child's content-based size (already calculated in bottom-up pass)
            base_size = is_row ? child->width : child->height;
        }

        base_sizes.push_back(base_size);
        total_base += base_size;
        total_grow += child->element->style.flex_grow;
        total_shrink += child->element->style.flex_shrink;
    }
    
    float remaining = available_main - total_base;
    
    // Distribute space
    for (size_t i = 0; i < node->children.size(); i++) {
        auto* child = node->children[i];
        
        // Skip absolute positioned elements
        if (child->element->style.position == Position::ABSOLUTE || 
            child->element->style.position == Position::FIXED) continue;
            
        float final_size = base_sizes[i];
        
        if (remaining > 0 && total_grow > 0) {
            final_size += (child->element->style.flex_grow / total_grow) * remaining;
        } else if (remaining < 0 && total_shrink > 0) {
            final_size += (child->element->style.flex_shrink / total_shrink) * remaining;
            final_size = std::max(0.0f, final_size);
        }
        
        // Set dimensions
        AlignItems align = node->element->style.align_items;
        bool should_stretch = (align == AlignItems::STRETCH);

        if (is_row) {
            child->width = final_size;
            // For cross-axis (height in row mode):
            // - Use explicit height if specified
            // - Keep content-based height if child has intrinsic size
            // - Stretch only if align-items: stretch AND no intrinsic size
            if (!child->constraints.height.is_auto()) {
                child->height = resolve_length(child->constraints.height, available_cross, 16.0f);
            } else if (child->height > 0) {
                // Keep the child's content-based height (already calculated in bottom-up pass)
            } else if (should_stretch || child->height <= 0) {
                // Stretch if align-items: stretch, OR if child has no intrinsic height (fallback)
                child->height = available_cross;
            }
        } else {
            child->height = final_size;
            // For cross-axis (width in column mode):
            if (!child->constraints.width.is_auto()) {
                child->width = resolve_length(child->constraints.width, available_cross, 16.0f);
            } else if (child->width > 0) {
                // Keep the child's content-based width (already calculated in bottom-up pass)
            } else if (should_stretch || child->width <= 0) {
                // Stretch if align-items: stretch, OR if child has no intrinsic width (fallback)
                child->width = available_cross;
            }
        }
    }
}

void QuadtreeLayoutEngine::calculate_flex_multi_line(LayoutNode* node, bool is_row, float available_main, float available_cross) {
    // Group children into lines
    std::vector<std::vector<size_t>> lines;
    std::vector<size_t> current_line;
    float current_line_size = 0;
    
    for (size_t i = 0; i < node->children.size(); i++) {
        auto* child = node->children[i];
        
        // Skip absolute positioned elements
        if (child->element->style.position == Position::ABSOLUTE || 
            child->element->style.position == Position::FIXED) continue;
        
        // Calculate base size
        float base_size = 0;
        if (!child->element->style.flex_basis.is_auto()) {
            base_size = resolve_length(child->element->style.flex_basis, available_main, 16.0f);
        } else if (is_row && !child->constraints.width.is_auto()) {
            base_size = resolve_length(child->constraints.width, available_main, 16.0f);
        } else if (!is_row && !child->constraints.height.is_auto()) {
            base_size = resolve_length(child->constraints.height, available_main, 16.0f);
        } else {
            // Use child's content-based size
            base_size = is_row ? child->width : child->height;
        }

        // Check if item fits on current line
        if (current_line.empty() || current_line_size + base_size <= available_main) {
            current_line.push_back(i);
            current_line_size += base_size;
        } else {
            // Start new line
            lines.push_back(current_line);
            current_line = {i};
            current_line_size = base_size;
        }
    }
    if (!current_line.empty()) {
        lines.push_back(current_line);
    }
    
    // Calculate dimensions for each line
    for (auto& line : lines) {
        float line_total_grow = 0;
        float line_total_base = 0;
        
        for (size_t idx : line) {
            auto* child = node->children[idx];
            float base_size = 0;

            if (!child->element->style.flex_basis.is_auto()) {
                base_size = resolve_length(child->element->style.flex_basis, available_main, 16.0f);
            } else if (is_row && !child->constraints.width.is_auto()) {
                base_size = resolve_length(child->constraints.width, available_main, 16.0f);
            } else if (!is_row && !child->constraints.height.is_auto()) {
                base_size = resolve_length(child->constraints.height, available_main, 16.0f);
            } else {
                base_size = is_row ? child->width : child->height;
            }

            line_total_base += base_size;
            line_total_grow += child->element->style.flex_grow;
        }
        
        float line_remaining = available_main - line_total_base;
        
        // Distribute space within line
        for (size_t idx : line) {
            auto* child = node->children[idx];
            float base_size = 0;

            if (!child->element->style.flex_basis.is_auto()) {
                base_size = resolve_length(child->element->style.flex_basis, available_main, 16.0f);
            } else if (is_row && !child->constraints.width.is_auto()) {
                base_size = resolve_length(child->constraints.width, available_main, 16.0f);
            } else if (!is_row && !child->constraints.height.is_auto()) {
                base_size = resolve_length(child->constraints.height, available_main, 16.0f);
            } else {
                base_size = is_row ? child->width : child->height;
            }

            float final_size = base_size;
            if (line_remaining > 0 && line_total_grow > 0) {
                final_size += (child->element->style.flex_grow / line_total_grow) * line_remaining;
            }
            
            // Set dimensions
            float line_cross = available_cross / lines.size();
            AlignItems align = node->element->style.align_items;
            bool should_stretch = (align == AlignItems::STRETCH);

            if (is_row) {
                child->width = final_size;
                if (!child->constraints.height.is_auto()) {
                    child->height = resolve_length(child->constraints.height, line_cross, 16.0f);
                } else if (child->height > 0 && (child->type == LayoutNode::FLEX || child->type == LayoutNode::GRID)) {
                    // Keep content-based height
                } else if (should_stretch) {
                    child->height = line_cross;
                }
            } else {
                child->height = final_size;
                if (!child->constraints.width.is_auto()) {
                    child->width = resolve_length(child->constraints.width, line_cross, 16.0f);
                } else if (child->width > 0 && (child->type == LayoutNode::FLEX || child->type == LayoutNode::GRID)) {
                    // Keep content-based width
                } else if (should_stretch) {
                    child->width = line_cross;
                }
            }
        }
    }
}

// Helper: Build a map from named grid lines to their line numbers
// Line numbers are 1-based (CSS convention)
// Example: [sidebar-start] 200px [sidebar-end content-start] 1fr [content-end]
//   -> {"sidebar-start": 1, "sidebar-end": 2, "content-start": 2, "content-end": 3}
static std::map<std::string, int> build_line_name_map(const std::vector<GridTrack>& tracks) {
    std::map<std::string, int> line_map;
    int line_num = 1;

    for (const auto& track : tracks) {
        // Names on this track refer to the line BEFORE the track
        for (const auto& name : track.line_names) {
            line_map[name] = line_num;
        }

        // Check if this is a trailing-names-only marker
        if (track.value == -1 && track.type == GridTrack::Type::AUTO && !track.line_names.empty()) {
            // This is just trailing names, don't increment line number
            continue;
        }

        line_num++;
    }

    return line_map;
}

// Helper: Resolve a named line reference to a line number
// Returns 0 if the name is not found (caller should treat as auto)
static int resolve_named_line(const std::string& name, const std::map<std::string, int>& line_map) {
    auto it = line_map.find(name);
    return (it != line_map.end()) ? it->second : 0;
}

void QuadtreeLayoutEngine::calculate_grid_dimensions(LayoutNode* node, cssboxRenderer* renderer) {
    // Sort children by order
    if (!node->children.empty()) {
        std::stable_sort(node->children.begin(), node->children.end(),
            [](const LayoutNode* a, const LayoutNode* b) {
                return a->element->style.order < b->element->style.order;
            });
    }

    // Container dimensions
    if (!node->constraints.width.is_auto()) {
        node->width = resolve_length(node->constraints.width, node->constraints.available_width, 16.0f);
        if (node->element->style.box_sizing == BoxSizing::CONTENT_BOX) {
            node->width += node->constraints.padding[1] + node->constraints.padding[3] +
                           node->constraints.border[1] + node->constraints.border[3];
        }
    } else {
        node->width = node->constraints.available_width;
    }

    if (!node->constraints.height.is_auto()) {
        node->height = resolve_length(node->constraints.height, node->constraints.available_height, 16.0f);
        if (node->element->style.box_sizing == BoxSizing::CONTENT_BOX) {
            node->height += node->constraints.padding[0] + node->constraints.padding[2] +
                            node->constraints.border[0] + node->constraints.border[2];
        }
    } else {
        node->height = node->constraints.available_height;
    }

    // Check if CSS string properties are used (only parse if inline_style is non-empty)
    bool has_css_strings = !node->element->inline_style.empty();

    // --- Use typed grid-template-areas from ComputedStyle (preferred) ---
    // Falls back to string-based parsing only if typed is empty and inline_style exists
    std::map<std::string, GridArea> areas;
    bool dense = false;
    bool flow_col = false;

    // First, check typed properties (fast path)
    const auto& typed_areas = node->element->style.grid_template_areas;
    if (!typed_areas.empty()) {
        for (const auto& [name, def] : typed_areas) {
            areas[name] = {def.row_start, def.col_start, def.row_end, def.col_end};
        }
    } else if (has_css_strings) {
        // Fallback to string-based parsing for backward compatibility
        std::string areas_str = get_string_property(renderer, node->element, "grid-template-areas");
        std::string auto_flow_str = get_string_property(renderer, node->element, "grid-auto-flow");

        // Parse grid-template-areas
        if (!areas_str.empty()) {
            std::istringstream iss(areas_str);
            std::string line;
            int row = 0;
            while (std::getline(iss, line, '"')) {
                if (line.find_first_not_of(" \t\n\r") == std::string::npos) continue;

                std::istringstream lss(line);
                std::string area_name;
                int col = 0;
                while (lss >> area_name) {
                    if (area_name != ".") {
                        if (areas.find(area_name) == areas.end()) {
                            areas[area_name] = {row, col, row + 1, col + 1};
                        } else {
                            auto& area = areas[area_name];
                            area.row_end = std::max(area.row_end, row + 1);
                            area.col_end = std::max(area.col_end, col + 1);
                        }
                    }
                    col++;
                }
                row++;
            }
        }

        dense = (auto_flow_str.find("dense") != std::string::npos);
        flow_col = (auto_flow_str.find("column") != std::string::npos);
    }

    // Determine initial track counts
    auto& explicit_cols = node->element->style.grid_template_columns;
    auto& explicit_rows = node->element->style.grid_template_rows;

    // Build named line maps for resolving named line references
    auto col_line_map = build_line_name_map(explicit_cols);
    auto row_line_map = build_line_name_map(explicit_rows);

    // Adjust track count to exclude trailing-names-only markers
    int num_cols = static_cast<int>(explicit_cols.size());
    int num_rows = static_cast<int>(explicit_rows.size());

    // Remove trailing marker from count if present
    if (num_cols > 0 && explicit_cols.back().value == -1 &&
        explicit_cols.back().type == GridTrack::Type::AUTO) {
        num_cols--;
    }
    if (num_rows > 0 && explicit_rows.back().value == -1 &&
        explicit_rows.back().type == GridTrack::Type::AUTO) {
        num_rows--;
    }

    // OPTIMIZATION: Single pass to collect placement info and determine grid size
    struct ChildPlacement {
        int row_start = -1, row_end = -1;
        int col_start = -1, col_end = -1;
        int row_span = 1, col_span = 1;
        bool has_explicit_row = false;
        bool has_explicit_col = false;
        bool is_positioned = false;  // Skip absolute/fixed elements
    };
    std::vector<ChildPlacement> placements(node->children.size());

    for (size_t i = 0; i < node->children.size(); i++) {
        auto* child = node->children[i];
        auto& p = placements[i];

        // Mark absolute positioned elements
        if (child->element->style.position == Position::ABSOLUTE ||
            child->element->style.position == Position::FIXED) {
            p.is_positioned = true;
            continue;
        }

        auto& style = child->element->style;

        // 1. Try typed style first (fast path - no string operations)
        // Check for named line references first
        if (style.grid_row.has_named_start()) {
            int line = resolve_named_line(style.grid_row.start_name, row_line_map);
            if (line > 0) {
                p.row_start = line - 1;  // Convert to 0-based
                p.has_explicit_row = true;
            }
        } else if (style.grid_row.start > 0) {
            p.row_start = style.grid_row.start - 1;
            p.has_explicit_row = true;
        }

        if (style.grid_row.has_named_end()) {
            int line = resolve_named_line(style.grid_row.end_name, row_line_map);
            if (line > 0) {
                p.row_end = line - 1;
            }
        } else if (style.grid_row.end > 0) {
            p.row_end = style.grid_row.end - 1;
        } else if (style.grid_row.span > 0) {
            p.row_span = style.grid_row.span;
        }

        if (style.grid_column.has_named_start()) {
            int line = resolve_named_line(style.grid_column.start_name, col_line_map);
            if (line > 0) {
                p.col_start = line - 1;  // Convert to 0-based
                p.has_explicit_col = true;
            }
        } else if (style.grid_column.start > 0) {
            p.col_start = style.grid_column.start - 1;
            p.has_explicit_col = true;
        }

        if (style.grid_column.has_named_end()) {
            int line = resolve_named_line(style.grid_column.end_name, col_line_map);
            if (line > 0) {
                p.col_end = line - 1;
            }
        } else if (style.grid_column.end > 0) {
            p.col_end = style.grid_column.end - 1;
        } else if (style.grid_column.span > 0) {
            p.col_span = style.grid_column.span;
        }

        // 2. Check typed grid_area property first (fast path)
        if (!p.has_explicit_row || !p.has_explicit_col) {
            const std::string& typed_grid_area = style.grid_area;
            if (!typed_grid_area.empty() && areas.count(typed_grid_area)) {
                auto& area = areas[typed_grid_area];
                p.row_start = area.row_start;
                p.row_end = area.row_end;
                p.col_start = area.col_start;
                p.col_end = area.col_end;
                p.has_explicit_row = true;
                p.has_explicit_col = true;
            }
        }

        // 3. Fall back to CSS string properties if typed didn't provide placement
        //    AND the element has inline CSS styles
        bool needs_css_lookup = (!p.has_explicit_row || !p.has_explicit_col) &&
                                !child->element->inline_style.empty();

        if (needs_css_lookup) {
            // Check grid-area string property
            std::string area_name = get_string_property(renderer, child->element, "grid-area");
            if (!area_name.empty() && areas.count(area_name)) {
                auto& area = areas[area_name];
                p.row_start = area.row_start;
                p.row_end = area.row_end;
                p.col_start = area.col_start;
                p.col_end = area.col_end;
                p.has_explicit_row = true;
                p.has_explicit_col = true;
            } else {
                // Try individual string properties
                if (!p.has_explicit_row) {
                    std::string rs = get_string_property(renderer, child->element, "grid-row-start");
                    if (!rs.empty()) {
                        p.row_start = std::atoi(rs.c_str()) - 1;
                        p.has_explicit_row = true;
                    }
                }
                if (p.has_explicit_row && p.row_end == -1 && p.row_span == 1) {
                    std::string re = get_string_property(renderer, child->element, "grid-row-end");
                    if (!re.empty()) {
                        if (re.find("span") != std::string::npos) {
                            size_t span_pos = re.find("span");
                            std::string val = re.substr(span_pos + 4);
                            p.row_span = std::atoi(val.c_str());
                        } else {
                            p.row_end = std::atoi(re.c_str()) - 1;
                        }
                    }
                }

                if (!p.has_explicit_col) {
                    std::string cs = get_string_property(renderer, child->element, "grid-column-start");
                    if (!cs.empty()) {
                        p.col_start = std::atoi(cs.c_str()) - 1;
                        p.has_explicit_col = true;
                    }
                }
                if (p.has_explicit_col && p.col_end == -1 && p.col_span == 1) {
                    std::string ce = get_string_property(renderer, child->element, "grid-column-end");
                    if (!ce.empty()) {
                        if (ce.find("span") != std::string::npos) {
                            size_t span_pos = ce.find("span");
                            std::string val = ce.substr(span_pos + 4);
                            p.col_span = std::atoi(val.c_str());
                        } else {
                            p.col_end = std::atoi(ce.c_str()) - 1;
                        }
                    }
                }

                // Legacy span properties
                std::string cspan = get_string_property(renderer, child->element, "grid-column-span");
                if (!cspan.empty()) p.col_span = std::atoi(cspan.c_str());

                std::string rspan = get_string_property(renderer, child->element, "grid-row-span");
                if (!rspan.empty()) p.row_span = std::atoi(rspan.c_str());
            }
        }

        // Resolve end from start + span if needed
        if (p.has_explicit_row && p.row_end == -1) p.row_end = p.row_start + p.row_span;
        if (p.has_explicit_col && p.col_end == -1) p.col_end = p.col_start + p.col_span;

        // Expand grid if needed for explicit placement
        if (p.has_explicit_row) num_rows = std::max(num_rows, p.row_end);
        if (p.has_explicit_col) num_cols = std::max(num_cols, p.col_end);
    }

    // Estimate final grid size for auto-placement (may grow)
    int estimated_rows = num_rows;
    int estimated_cols = num_cols;

    // Count auto-placed items to estimate grid growth
    int auto_items = 0;
    for (const auto& p : placements) {
        if (!p.is_positioned && (!p.has_explicit_row || !p.has_explicit_col)) {
            auto_items++;
        }
    }

    // If we have auto items, ensure grid has at least 1 column
    if (auto_items > 0 && estimated_cols == 0) {
        estimated_cols = 1;
    }

    // Estimate rows needed for auto-placed items
    if (auto_items > 0 && estimated_cols > 0) {
        int needed_rows = (auto_items + estimated_cols - 1) / estimated_cols;
        estimated_rows = std::max(estimated_rows, needed_rows);
    }

    // Add safety margin for spanning items
    estimated_rows = std::max(estimated_rows, num_rows + 10);
    estimated_cols = std::max(estimated_cols, num_cols + 10);

    // OPTIMIZATION: Use flat bitmap for O(1) access with cache-friendly memory layout
    // Each uint64_t stores 64 cells, packed row-major
    const int bitmap_cols = estimated_cols;
    const int words_per_row = (bitmap_cols + 63) / 64;  // Round up to 64-bit boundary
    std::vector<uint64_t> bitmap(estimated_rows * words_per_row, 0);
    int bitmap_rows = estimated_rows;

    // Helper to resize bitmap if needed
    auto ensure_bitmap_size = [&](int r, int c) {
        bool needs_resize = false;
        int new_rows = bitmap_rows;
        int new_words_per_row = words_per_row;

        if (r >= bitmap_rows) {
            new_rows = r + 10;
            needs_resize = true;
        }
        if (c >= bitmap_cols) {
            // Need more columns - this is expensive, avoid if possible
            new_words_per_row = (c + 10 + 63) / 64;
            needs_resize = true;
        }

        if (needs_resize) {
            std::vector<uint64_t> new_bitmap(new_rows * new_words_per_row, 0);
            // Copy old data
            for (int row = 0; row < bitmap_rows; row++) {
                for (int w = 0; w < words_per_row; w++) {
                    new_bitmap[row * new_words_per_row + w] = bitmap[row * words_per_row + w];
                }
            }
            bitmap = std::move(new_bitmap);
            bitmap_rows = new_rows;
        }
    };

    auto is_occupied = [&](int r, int c) -> bool {
        if (r < 0 || c < 0) return true;
        if (r >= bitmap_rows || c >= bitmap_cols) return false;
        int word_idx = r * words_per_row + (c / 64);
        int bit_idx = c % 64;
        return (bitmap[word_idx] & (1ULL << bit_idx)) != 0;
    };

    auto set_occupied = [&](int r, int c) {
        ensure_bitmap_size(r, c);
        int word_idx = r * words_per_row + (c / 64);
        int bit_idx = c % 64;
        bitmap[word_idx] |= (1ULL << bit_idx);
    };

    // OPTIMIZATION: Single pass for placement with merged logic
    // Phase 1: Place explicitly positioned items
    for (size_t i = 0; i < node->children.size(); i++) {
        auto& p = placements[i];
        if (p.is_positioned) continue;

        if (p.has_explicit_row && p.has_explicit_col) {
            for (int r = p.row_start; r < p.row_end; r++) {
                for (int c = p.col_start; c < p.col_end; c++) {
                    set_occupied(r, c);
                }
            }
        }
    }

    // Phase 2: Place items with partial explicit placement + fully auto items
    int cursor_row = 0;
    int cursor_col = 0;

    for (size_t i = 0; i < node->children.size(); i++) {
        auto& p = placements[i];
        if (p.is_positioned) continue;
        if (p.has_explicit_row && p.has_explicit_col) continue;  // Already placed

        if (p.has_explicit_row && !p.has_explicit_col) {
            // Explicit row, auto column
            int c = 0;
            while (true) {
                bool fits = true;
                for (int r = p.row_start; r < p.row_end && fits; r++) {
                    for (int dc = 0; dc < p.col_span && fits; dc++) {
                        if (is_occupied(r, c + dc)) fits = false;
                    }
                }
                if (fits) break;
                c++;
                if (c > 10000) break;  // Safety limit
            }
            p.col_start = c;
            p.col_end = c + p.col_span;
            p.has_explicit_col = true;

            for (int r = p.row_start; r < p.row_end; r++) {
                for (int dc = 0; dc < p.col_span; dc++) {
                    set_occupied(r, p.col_start + dc);
                }
            }
            num_cols = std::max(num_cols, p.col_end);

        } else if (p.has_explicit_col && !p.has_explicit_row) {
            // Explicit col, auto row
            int r = 0;
            while (true) {
                bool fits = true;
                for (int c = p.col_start; c < p.col_end && fits; c++) {
                    for (int dr = 0; dr < p.row_span && fits; dr++) {
                        if (is_occupied(r + dr, c)) fits = false;
                    }
                }
                if (fits) break;
                r++;
                if (r > 10000) break;  // Safety limit
            }
            p.row_start = r;
            p.row_end = r + p.row_span;
            p.has_explicit_row = true;

            for (int c = p.col_start; c < p.col_end; c++) {
                for (int dr = 0; dr < p.row_span; dr++) {
                    set_occupied(p.row_start + dr, c);
                }
            }
            num_rows = std::max(num_rows, p.row_end);

        } else {
            // Fully auto placement
            if (dense) {
                cursor_row = 0;
                cursor_col = 0;
            }

            while (true) {
                // Check wrapping for row-flow
                if (!flow_col && num_cols > 0 && cursor_col + p.col_span > num_cols) {
                    cursor_row++;
                    cursor_col = 0;
                }

                bool fits = true;
                for (int dr = 0; dr < p.row_span && fits; dr++) {
                    for (int dc = 0; dc < p.col_span && fits; dc++) {
                        if (is_occupied(cursor_row + dr, cursor_col + dc)) {
                            fits = false;
                        }
                    }
                }

                if (fits) {
                    p.row_start = cursor_row;
                    p.row_end = cursor_row + p.row_span;
                    p.col_start = cursor_col;
                    p.col_end = cursor_col + p.col_span;

                    for (int dr = 0; dr < p.row_span; dr++) {
                        for (int dc = 0; dc < p.col_span; dc++) {
                            set_occupied(cursor_row + dr, cursor_col + dc);
                        }
                    }

                    num_rows = std::max(num_rows, p.row_end);
                    num_cols = std::max(num_cols, p.col_end);

                    if (!dense) cursor_col += p.col_span;
                    break;
                }

                cursor_col++;
                if (num_cols > 0 && cursor_col >= num_cols && !flow_col) {
                    cursor_row++;
                    cursor_col = 0;
                }

                if (cursor_row > 10000) break;  // Safety limit
            }
        }
    }

    // Now calculate track sizes
    float gap_h = resolve_length(node->element->style.grid_column_gap, node->width, 16.0f);
    float gap_v = resolve_length(node->element->style.grid_row_gap, node->height, 16.0f);

    // Available space for content
    float available_w = node->width - node->constraints.padding[1] - node->constraints.padding[3];
    float available_h = node->height - node->constraints.padding[0] - node->constraints.padding[2];

    // Helper to resolve GridSizeValue
    auto resolve_grid_size = [&](const GridSizeValue& size, float available, float content_min, float content_max) -> float {
        switch (size.type) {
            case GridSize::PX: return size.value;
            case GridSize::PERCENT: return size.value / 100.0f * available;
            case GridSize::FR: return 0;  // FR handled separately
            case GridSize::MIN_CONTENT: return content_min;
            case GridSize::MAX_CONTENT: return content_max;
            case GridSize::AUTO:
            default: return content_max;  // auto behaves like max-content for sizing
        }
    };

    // Helper to get track's base size (non-FR part)
    auto get_track_base_size = [&](const GridTrack& track, float available, float content_min, float content_max) -> float {
        switch (track.type) {
            case GridTrack::Type::PX:
                return track.value;
            case GridTrack::Type::FR:
                return 0;  // FR tracks start at 0, filled later
            case GridTrack::Type::MIN_CONTENT:
                return content_min;
            case GridTrack::Type::MAX_CONTENT:
                return content_max;
            case GridTrack::Type::FIT_CONTENT:
                return std::min(content_max, track.value);  // fit-content(limit)
            case GridTrack::Type::MINMAX: {
                float min_val = resolve_grid_size(track.min_size, available, content_min, content_max);
                float max_val = resolve_grid_size(track.max_size, available, content_min, content_max);
                // For base size, use min value; max is used for growth
                return min_val;
            }
            case GridTrack::Type::AUTO:
            default:
                return content_min;  // auto starts at min-content
        }
    };

    // Helper to check if track can grow (has FR or flexible max)
    auto get_track_fr = [](const GridTrack& track) -> float {
        if (track.type == GridTrack::Type::FR) return track.value;
        if (track.type == GridTrack::Type::MINMAX && track.max_size.type == GridSize::FR) {
            return track.max_size.value;
        }
        return 0;
    };

    // Helper to get track's max size limit
    auto get_track_max_size = [&](const GridTrack& track, float available, float content_min, float content_max) -> float {
        switch (track.type) {
            case GridTrack::Type::PX:
                return track.value;
            case GridTrack::Type::FR:
                return std::numeric_limits<float>::max();  // FR can grow infinitely
            case GridTrack::Type::MIN_CONTENT:
                return content_min;
            case GridTrack::Type::MAX_CONTENT:
                return content_max;
            case GridTrack::Type::FIT_CONTENT:
                return std::min(content_max, track.value);
            case GridTrack::Type::MINMAX: {
                if (track.max_size.type == GridSize::FR) {
                    return std::numeric_limits<float>::max();
                }
                return resolve_grid_size(track.max_size, available, content_min, content_max);
            }
            case GridTrack::Type::AUTO:
            default:
                return content_max;
        }
    };

    // Build full track list (explicit + implicit)
    std::vector<float> col_sizes(num_cols);
    std::vector<float> row_sizes(num_rows);

    // =========================================================================
    // CONTENT MEASUREMENT: Measure actual grid item intrinsic sizes
    // =========================================================================
    // Per-track content sizes: min-content and max-content for each track
    std::vector<float> col_min_content(num_cols, 0.0f);
    std::vector<float> col_max_content(num_cols, 0.0f);
    std::vector<float> row_min_content(num_rows, 0.0f);
    std::vector<float> row_max_content(num_rows, 0.0f);

    // Helper to measure a single item's intrinsic size
    auto measure_item_intrinsic = [&](LayoutNode* child, float& min_w, float& max_w,
                                       float& min_h, float& max_h) {
        min_w = 0.0f;
        max_w = 0.0f;
        min_h = 0.0f;
        max_h = 0.0f;

        // 1. Measure text content (with caching and string_view optimization)
        if (!child->element->text_content.empty() && renderer && renderer->vg) {
            NVGcontext* vg = renderer->vg;

            // Setup font (use cached resolution)
            const std::string& font_face = get_resolved_font(
                child->element->style.font_family,
                child->element->style.font_weight
            );
            float font_size = child->element->style.font_size;
            nvgFontFace(vg, font_face.c_str());
            nvgFontSize(vg, font_size);
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

            // max-content: Single line, no wrapping (use cached measurement)
            auto max_bounds = get_text_bounds_cached(
                vg, child->element->text_content, font_size, font_face
            );
            max_w = max_bounds.width;
            max_h = max_bounds.height;

            // min-content: Measure longest word using string_view (zero-copy)
            min_w = 0.0f;
            std::string_view text(child->element->text_content);
            size_t start = 0;
            while (start < text.size()) {
                size_t end = text.find(' ', start);
                if (end == std::string_view::npos) end = text.size();
                if (end > start) {
                    std::string_view word = text.substr(start, end - start);
                    float word_w = measure_word_width(vg, word, font_size, font_face);
                    min_w = std::max(min_w, word_w);
                }
                start = end + 1;
            }
            min_h = max_h;  // Single line height for min-content
        }

        // 2. Check explicit size constraints
        if (!child->constraints.width.is_auto()) {
            float explicit_w = resolve_length(child->constraints.width, available_w, 16.0f);
            min_w = std::max(min_w, explicit_w);
            max_w = std::max(max_w, explicit_w);
        }
        if (!child->constraints.height.is_auto()) {
            float explicit_h = resolve_length(child->constraints.height, available_h, 16.0f);
            min_h = std::max(min_h, explicit_h);
            max_h = std::max(max_h, explicit_h);
        }

        // 3. Check min-width/min-height constraints
        if (!child->constraints.min_width.is_auto()) {
            float min_constraint = resolve_length(child->constraints.min_width, available_w, 16.0f);
            min_w = std::max(min_w, min_constraint);
            max_w = std::max(max_w, min_constraint);
        }
        if (!child->constraints.min_height.is_auto()) {
            float min_constraint = resolve_length(child->constraints.min_height, available_h, 16.0f);
            min_h = std::max(min_h, min_constraint);
            max_h = std::max(max_h, min_constraint);
        }

        // 4. Add padding and border
        float pad_h = child->constraints.padding[1] + child->constraints.padding[3] +
                      child->constraints.border[1] + child->constraints.border[3];
        float pad_v = child->constraints.padding[0] + child->constraints.padding[2] +
                      child->constraints.border[0] + child->constraints.border[2];
        min_w += pad_h;
        max_w += pad_h;
        min_h += pad_v;
        max_h += pad_v;

        // 5. If child is a flex/grid container with children, measure recursively
        if (!child->children.empty()) {
            float children_min_w = 0, children_max_w = 0;
            float children_min_h = 0, children_max_h = 0;

            bool is_row_flex = child->type == LayoutNode::FLEX &&
                              (child->element->style.flex_direction == FlexDirection::ROW ||
                               child->element->style.flex_direction == FlexDirection::ROW_REVERSE);

            for (auto* grandchild : child->children) {
                if (grandchild->element->style.position == Position::ABSOLUTE ||
                    grandchild->element->style.position == Position::FIXED) continue;

                // Use already-calculated child dimensions as approximation
                float gw = grandchild->width;
                float gh = grandchild->height;

                if (is_row_flex) {
                    // Row flex: sum widths, max height
                    children_min_w += gw;
                    children_max_w += gw;
                    children_min_h = std::max(children_min_h, gh);
                    children_max_h = std::max(children_max_h, gh);
                } else {
                    // Column flex or block: max width, sum heights
                    children_min_w = std::max(children_min_w, gw);
                    children_max_w = std::max(children_max_w, gw);
                    children_min_h += gh;
                    children_max_h += gh;
                }
            }

            min_w = std::max(min_w, children_min_w + pad_h);
            max_w = std::max(max_w, children_max_w + pad_h);
            min_h = std::max(min_h, children_min_h + pad_v);
            max_h = std::max(max_h, children_max_h + pad_v);
        }
    };

    // Measure all grid items and distribute to tracks
    for (size_t i = 0; i < node->children.size(); i++) {
        auto* child = node->children[i];
        auto& p = placements[i];

        if (p.is_positioned) continue;

        float min_w, max_w, min_h, max_h;
        measure_item_intrinsic(child, min_w, max_w, min_h, max_h);

        // Distribute content size to spanned tracks
        // For spanning items, we contribute proportionally
        int col_span = p.col_end - p.col_start;
        int row_span = p.row_end - p.row_start;

        if (col_span == 1 && p.col_start < num_cols) {
            // Single column: direct contribution
            col_min_content[p.col_start] = std::max(col_min_content[p.col_start], min_w);
            col_max_content[p.col_start] = std::max(col_max_content[p.col_start], max_w);
        } else if (col_span > 1) {
            // Spanning multiple columns: distribute evenly (simplified)
            // CSS Grid spec uses more complex algorithms, but this is practical
            float per_col_min = min_w / col_span;
            float per_col_max = max_w / col_span;
            for (int c = p.col_start; c < p.col_end && c < num_cols; c++) {
                col_min_content[c] = std::max(col_min_content[c], per_col_min);
                col_max_content[c] = std::max(col_max_content[c], per_col_max);
            }
        }

        if (row_span == 1 && p.row_start < num_rows) {
            row_min_content[p.row_start] = std::max(row_min_content[p.row_start], min_h);
            row_max_content[p.row_start] = std::max(row_max_content[p.row_start], max_h);
        } else if (row_span > 1) {
            float per_row_min = min_h / row_span;
            float per_row_max = max_h / row_span;
            for (int r = p.row_start; r < p.row_end && r < num_rows; r++) {
                row_min_content[r] = std::max(row_min_content[r], per_row_min);
                row_max_content[r] = std::max(row_max_content[r], per_row_max);
            }
        }
    }

    // Calculate column sizes using measured content
    float total_fixed_width = 0;
    float total_fr_cols = 0;

    for (int i = 0; i < num_cols; i++) {
        GridTrack track = (i < static_cast<int>(explicit_cols.size()))
            ? explicit_cols[i]
            : node->element->style.grid_auto_columns;  // Use grid-auto-columns for implicit
        // Use per-track measured content sizes
        float content_min = col_min_content[i];
        float content_max = col_max_content[i];
        float base = get_track_base_size(track, available_w, content_min, content_max);
        col_sizes[i] = base;
        total_fixed_width += base;
        total_fr_cols += get_track_fr(track);
    }

    // Calculate row sizes using measured content
    float total_fixed_height = 0;
    float total_fr_rows = 0;

    for (int i = 0; i < num_rows; i++) {
        GridTrack track = (i < static_cast<int>(explicit_rows.size()))
            ? explicit_rows[i]
            : node->element->style.grid_auto_rows;  // Use grid-auto-rows for implicit
        // Use per-track measured content sizes
        float content_min = row_min_content[i];
        float content_max = row_max_content[i];
        float base = get_track_base_size(track, available_h, content_min, content_max);
        row_sizes[i] = base;
        total_fixed_height += base;
        total_fr_rows += get_track_fr(track);
    }

    // Distribute remaining space to FR tracks
    if (num_cols > 1) available_w -= gap_h * (num_cols - 1);
    float remaining_w = available_w - total_fixed_width;
    if (total_fr_cols > 0 && remaining_w > 0) {
        float unit = remaining_w / total_fr_cols;
        for (int i = 0; i < num_cols; i++) {
            GridTrack track = (i < static_cast<int>(explicit_cols.size()))
                ? explicit_cols[i]
                : node->element->style.grid_auto_columns;
            float fr = get_track_fr(track);
            if (fr > 0) {
                float growth = fr * unit;
                float content_min = col_min_content[i];
                float content_max = col_max_content[i];
                float max_size = get_track_max_size(track, available_w, content_min, content_max);
                col_sizes[i] = std::min(col_sizes[i] + growth, max_size);
            }
        }
    }

    if (num_rows > 1) available_h -= gap_v * (num_rows - 1);

    float remaining_h = available_h - total_fixed_height;
    if (total_fr_rows > 0 && remaining_h > 0) {
        float unit = remaining_h / total_fr_rows;
        for (int i = 0; i < num_rows; i++) {
            GridTrack track = (i < static_cast<int>(explicit_rows.size()))
                ? explicit_rows[i]
                : node->element->style.grid_auto_rows;
            float fr = get_track_fr(track);
            if (fr > 0) {
                float growth = fr * unit;
                float content_min = row_min_content[i];
                float content_max = row_max_content[i];
                float max_size = get_track_max_size(track, available_h, content_min, content_max);
                row_sizes[i] = std::min(row_sizes[i] + growth, max_size);
            }
        }
    }

    // Calculate dimensions for children
    for (size_t i = 0; i < node->children.size(); i++) {
        auto* child = node->children[i];

        if (child->element->style.position == Position::ABSOLUTE ||
            child->element->style.position == Position::FIXED) {
             // For absolute items in grid, we should calculate dimensions if they have explicit placement
             // But for now, let's treat them as standard absolute positioning (relative to padding box)
             // and skip grid track sizing.
             continue;
        }

        auto& p = placements[i];

        float cell_w = 0;
        for (int c = p.col_start; c < p.col_end; c++) {
            cell_w += col_sizes[c];
            if (c > p.col_start) cell_w += gap_h;
        }

        float cell_h = 0;
        for (int r = p.row_start; r < p.row_end; r++) {
            cell_h += row_sizes[r];
            if (r > p.row_start) cell_h += gap_v;
        }

        // === Grid Item Alignment ===
        // Determine effective alignment (item's self-alignment overrides container's items alignment)
        auto& child_style = child->element->style;
        auto& container_style = node->element->style;

        // Horizontal alignment (justify)
        JustifySelf h_align = child_style.justify_self;
        if (h_align == JustifySelf::AUTO) {
            // Inherit from container's justify-items
            switch (container_style.justify_items) {
                case JustifyItems::START: h_align = JustifySelf::START; break;
                case JustifyItems::END: h_align = JustifySelf::END; break;
                case JustifyItems::CENTER: h_align = JustifySelf::CENTER; break;
                default: h_align = JustifySelf::STRETCH; break;
            }
        }

        // Vertical alignment (align)
        AlignSelf v_align = child_style.align_self;
        if (v_align == AlignSelf::AUTO) {
            // Inherit from container's align-items
            switch (container_style.align_items) {
                case AlignItems::FLEX_START: v_align = AlignSelf::START; break;
                case AlignItems::FLEX_END: v_align = AlignSelf::END; break;
                case AlignItems::CENTER: v_align = AlignSelf::CENTER; break;
                case AlignItems::BASELINE: v_align = AlignSelf::BASELINE; break;
                default: v_align = AlignSelf::STRETCH; break;
            }
        }

        // Calculate child dimensions based on alignment
        float child_w, child_h;

        // Width: stretch or explicit/intrinsic
        if (h_align == JustifySelf::STRETCH && child->constraints.width.is_auto()) {
            child_w = cell_w;
        } else if (!child->constraints.width.is_auto()) {
            child_w = resolve_length(child->constraints.width, cell_w, 16.0f);
        } else {
            // Non-stretch with auto width: use min-content (approximate with 0 for now)
            // TODO: Implement proper intrinsic sizing
            child_w = cell_w;  // Fallback to cell size
        }

        // Height: stretch or explicit/intrinsic
        if (v_align == AlignSelf::STRETCH && child->constraints.height.is_auto()) {
            child_h = cell_h;
        } else if (!child->constraints.height.is_auto()) {
            child_h = resolve_length(child->constraints.height, cell_h, 16.0f);
        } else {
            // Non-stretch with auto height: use min-content
            child_h = cell_h;  // Fallback to cell size
        }

        child->width = child_w;
        child->height = child_h;

        // Calculate position offset within cell based on alignment
        float offset_x = 0, offset_y = 0;

        switch (h_align) {
            case JustifySelf::START:
                offset_x = 0;
                break;
            case JustifySelf::END:
                offset_x = cell_w - child_w;
                break;
            case JustifySelf::CENTER:
                offset_x = (cell_w - child_w) / 2.0f;
                break;
            case JustifySelf::STRETCH:
            case JustifySelf::AUTO:
            default:
                offset_x = 0;
                break;
        }

        switch (v_align) {
            case AlignSelf::START:
                offset_y = 0;
                break;
            case AlignSelf::END:
                offset_y = cell_h - child_h;
                break;
            case AlignSelf::CENTER:
                offset_y = (cell_h - child_h) / 2.0f;
                break;
            case AlignSelf::BASELINE:
                // TODO: Implement baseline alignment
                offset_y = 0;
                break;
            case AlignSelf::STRETCH:
            case AlignSelf::AUTO:
            default:
                offset_y = 0;
                break;
        }

        // Recalculate content dimensions for this grid item
        child->content_width = child->width
            - child->constraints.padding[1] - child->constraints.padding[3]
            - child->constraints.border[1] - child->constraints.border[3];
        child->content_height = child->height
            - child->constraints.padding[0] - child->constraints.padding[2]
            - child->constraints.border[0] - child->constraints.border[2];
        child->content_width = std::max(0.0f, child->content_width);
        child->content_height = std::max(0.0f, child->content_height);

        // IMPORTANT: Re-propagate cross-axis size to grandchildren
        // Grid items' children were calculated before grid assigned the parent's width
        // So we need to fix their cross-axis dimension now
        if (child->type == LayoutNode::FLEX) {
            bool is_row = (child->element->style.flex_direction == FlexDirection::ROW ||
                          child->element->style.flex_direction == FlexDirection::ROW_REVERSE);
            float available_cross = is_row ? child->content_height : child->content_width;
            AlignItems child_align = child->element->style.align_items;
            bool child_should_stretch = (child_align == AlignItems::STRETCH);

            for (auto* grandchild : child->children) {
                if (grandchild->element->style.position == Position::ABSOLUTE ||
                    grandchild->element->style.position == Position::FIXED) continue;

                if (is_row) {
                    // Row: cross-axis is height
                    if (!grandchild->constraints.height.is_auto()) {
                        // Re-resolve percentage heights against correct parent
                        grandchild->height = resolve_length(grandchild->constraints.height, available_cross, 16.0f);
                    } else if (child_should_stretch) {
                        grandchild->height = available_cross;
                    }
                } else {
                    // Column: cross-axis is width (most common case for flex-column)
                    if (!grandchild->constraints.width.is_auto()) {
                        // Re-resolve percentage widths (like width: 100%) against correct parent
                        grandchild->width = resolve_length(grandchild->constraints.width, available_cross, 16.0f);
                    } else if (child_should_stretch) {
                        grandchild->width = available_cross;
                    }
                }
            }
        }

        // Calculate cell position and apply alignment offset
        float x = 0;
        for (int c = 0; c < p.col_start; c++) x += col_sizes[c] + gap_h;

        float y = 0;
        for (int r = 0; r < p.row_start; r++) y += row_sizes[r] + gap_v;

        // Apply alignment offset within cell
        child->x = x + offset_x;
        child->y = y + offset_y;
    }
}

// ============================================================================
// Phase 3: Position Calculation (Top-Down)
// ============================================================================

void QuadtreeLayoutEngine::calculate_positions(LayoutNode* node, float parent_x, float parent_y) {
    // Position this node relative to parent
    node->x = parent_x;
    node->y = parent_y;

    // Override with explicit position if set (from CSS left/top properties)
    // Use typed properties directly instead of deprecated explicit_style
    if (!node->element->style.left.is_auto()) {
        float parent_abs_x = node->parent ? node->parent->x : 0.0f;
        float left_value = node->element->style.left.resolve(viewport_width_, 16.0f, viewport_width_);
        node->x = parent_abs_x + left_value;
    }

    if (!node->element->style.top.is_auto()) {
        float parent_abs_y = node->parent ? node->parent->y : 0.0f;
        float top_value = node->element->style.top.resolve(viewport_height_, 16.0f, viewport_height_);
        node->y = parent_abs_y + top_value;
    }
    
    // Position children based on layout type (sets relative positions within this node)
    switch (node->type) {
        case LayoutNode::FLEX:
            position_flex_children(node);
            break;
        case LayoutNode::GRID:
            position_grid_children(node);
            break;
        case LayoutNode::BLOCK:
        default:
            position_block_children(node);
            break;
    }
    
    // Handle absolute positioning for children that are ABSOLUTE/FIXED
    for (auto* child : node->children) {
        if (child->element->style.position == Position::ABSOLUTE ||
            child->element->style.position == Position::FIXED) {
            
            // Default position is padding box origin
            float x = node->constraints.padding[3] + node->constraints.border[3];
            float y = node->constraints.padding[0] + node->constraints.border[0];
            
            // Resolve Top/Left/Right/Bottom
            // Note: Simplification - assume LTR and relative to Top-Left
            
            if (!child->element->style.left.is_auto()) {
                x = node->constraints.border[3] + resolve_length(child->element->style.left, node->width, 16.0f);
            } else if (!child->element->style.right.is_auto()) {
                float right = resolve_length(child->element->style.right, node->width, 16.0f);
                x = node->width - node->constraints.padding[1] - node->constraints.border[1] - right - child->width;
            }
            
            if (!child->element->style.top.is_auto()) {
                y = node->constraints.border[0] + resolve_length(child->element->style.top, node->height, 16.0f);
            } else if (!child->element->style.bottom.is_auto()) {
                float bottom = resolve_length(child->element->style.bottom, node->height, 16.0f);
                y = node->height - node->constraints.padding[2] - node->constraints.border[2] - bottom - child->height;
            }
            
            child->x = x;
            child->y = y;
        }
    }
    
    // Recursively position children (child positions are already relative to this node)
    for (auto* child : node->children) {
        calculate_positions(child, node->x + child->x, node->y + child->y);
    }
}

void QuadtreeLayoutEngine::position_block_children(LayoutNode* node) {
    // Block layout: stack children vertically
    float y_offset = node->constraints.padding[0] + node->constraints.border[0];  // top padding + border
    
    for (auto* child : node->children) {
        // Skip absolute positioned elements
        if (child->element->style.position == Position::ABSOLUTE || 
            child->element->style.position == Position::FIXED) continue;

        child->x = node->constraints.padding[3] + node->constraints.border[3];  // left padding + border
        child->y = y_offset;
        
        y_offset += child->height + child->constraints.margin[0] + child->constraints.margin[2];
    }
}

void QuadtreeLayoutEngine::position_flex_children(LayoutNode* node) {
    bool is_row = (node->element->style.flex_direction == FlexDirection::ROW || 
                   node->element->style.flex_direction == FlexDirection::ROW_REVERSE);
    bool is_reverse = (node->element->style.flex_direction == FlexDirection::ROW_REVERSE || 
                       node->element->style.flex_direction == FlexDirection::COLUMN_REVERSE);
    bool can_wrap = (node->element->style.flex_wrap != FlexWrap::NOWRAP);
    
    float available_main = is_row ? node->content_width : node->content_height;
    // float available_cross = is_row ? node->content_height : node->content_width; // Not used yet

    float gap_value = resolve_length(node->element->style.gap, available_main, 16.0f);

    // Reconstruct lines (same logic as dimensions)
    std::vector<std::vector<size_t>> lines;
    std::vector<size_t> current_line;
    float current_line_size = 0;
    
    for (size_t i = 0; i < node->children.size(); i++) {
        auto* child = node->children[i];
        
        // Skip absolute positioned elements
        if (child->element->style.position == Position::ABSOLUTE || 
            child->element->style.position == Position::FIXED) continue;
        
        // Calculate size including margins (dimensions are already set in Phase 2)
        float item_size = is_row ? 
            (child->width + child->constraints.margin[1] + child->constraints.margin[3]) :
            (child->height + child->constraints.margin[0] + child->constraints.margin[2]);

        if (can_wrap) {
            // We must use the base size (basis) for wrapping calculation, not the final size.
            // But Phase 2 overwrote width/height with final size.
            // However, the sum of final sizes in a line should roughly equal available_main (if grown).
            // But 'wrap' logic decisions were made on base sizes.
            // If we use final sizes here, we might get different wrapping if we are not careful?
            // Actually, since we don't store line info, we have to approximate or re-calculate basis.
            // Re-calculating basis is safer.
            float base_size = 0;
             if (!child->element->style.flex_basis.is_auto()) {
                base_size = resolve_length(child->element->style.flex_basis, available_main, 16.0f);
            } else if (is_row && !child->constraints.width.is_auto()) {
                base_size = resolve_length(child->constraints.width, available_main, 16.0f);
            } else if (!is_row && !child->constraints.height.is_auto()) {
                base_size = resolve_length(child->constraints.height, available_main, 16.0f);
            }
            
            // Add margins to base size for wrapping check
            float margin_size = is_row ? 
                (child->constraints.margin[1] + child->constraints.margin[3]) :
                (child->constraints.margin[0] + child->constraints.margin[2]);
            
            if (current_line.empty() || current_line_size + base_size + margin_size <= available_main) {
                current_line.push_back(i);
                current_line_size += base_size + margin_size;
            } else {
                lines.push_back(current_line);
                current_line = {i};
                current_line_size = base_size + margin_size;
            }
        } else {
            current_line.push_back(i);
        }
    }
    if (!current_line.empty()) lines.push_back(current_line);
    
    // Cross axis positioning
    float cross_offset = is_row ? (node->constraints.padding[0] + node->constraints.border[0]) : 
                                  (node->constraints.padding[3] + node->constraints.border[3]);
    
    // For single line, we stretch to content height. For multi-line, we divide cross space?
    // Simplified: Just stack lines.
    // Real flexbox: align-content distributes lines. Default is stretch.
    // We'll assume lines just stack for now (align-content: flex-start/stretch behavior).
    
    for (auto& line : lines) {
        float line_cross_size = 0;
        // Determine line height (max child cross size)
        for (size_t idx : line) {
            auto* child = node->children[idx];
            float size = is_row ? 
                (child->height + child->constraints.margin[0] + child->constraints.margin[2]) :
                (child->width + child->constraints.margin[1] + child->constraints.margin[3]);
            line_cross_size = std::max(line_cross_size, size);
        }

        // If single line (nowrap), the line fills the cross axis
        if (!can_wrap) {
            float container_cross = is_row ? node->content_height : node->content_width;
            line_cross_size = std::max(line_cross_size, container_cross);
        }
        
        // Main axis positioning
        float line_main_size = 0;
        for (size_t idx : line) {
            auto* child = node->children[idx];
             float size = is_row ? 
                (child->width + child->constraints.margin[1] + child->constraints.margin[3]) :
                (child->height + child->constraints.margin[0] + child->constraints.margin[2]);
            line_main_size += size;
        }
        if (line.size() > 1) line_main_size += gap_value * (line.size() - 1);
        
        float remaining = available_main - line_main_size;
        float start_main = is_row ? (node->constraints.padding[3] + node->constraints.border[3]) :
                                    (node->constraints.padding[0] + node->constraints.border[0]);
                                    
        float gap_actual = gap_value;
        
        // Justify Content
        JustifyContent justify = node->element->style.justify_content;
        
        if (is_reverse) {
            // For reverse, swap start/end logic
            // flex-start -> packs to end (visually right)
            // flex-end -> packs to start (visually left)
            // But wait, standard says:
            // "row-reverse: Main-start is right. flex-start packs to right."
            // So start_main should be initialized to right edge if we want to iterate normally?
            // Or we just calculate offsets normally relative to main-start, then map to coordinates.
            
            // Let's keep logic relative to "main start".
            // For row-reverse: main start is at x + width. Direction is -x.
            // But easier: Calculate positions as if LTR, then flip X coordinate at the end?
            // Flip X: x' = width - x - child_width.
        }

        if (justify == JustifyContent::CENTER) {
            start_main += remaining / 2;
        } else if (justify == JustifyContent::FLEX_END) {
            start_main += remaining;
        } else if (justify == JustifyContent::SPACE_BETWEEN && line.size() > 1) {
            gap_actual = remaining / (line.size() - 1);
        } else if (justify == JustifyContent::SPACE_AROUND && line.size() > 0) {
            gap_actual = remaining / line.size();
            start_main += gap_actual / 2;
        } else if (justify == JustifyContent::SPACE_EVENLY && line.size() > 0) {
            gap_actual = remaining / (line.size() + 1);
            start_main += gap_actual;
        }
        
        // Place items
        float current_main = start_main;
        for (size_t idx : line) {
            auto* child = node->children[idx];
            AlignItems align = node->element->style.align_items;
            
            if (is_row) {
                // Cross alignment
                float child_y = cross_offset + child->constraints.margin[0]; // align: flex-start default
                if (align == AlignItems::CENTER) {
                    child_y = cross_offset + (line_cross_size - child->height) / 2;
                } else if (align == AlignItems::FLEX_END) {
                    child_y = cross_offset + (line_cross_size - child->height - child->constraints.margin[2]);
                }
                
                // Main position (handle reverse)
                float child_x;
                if (!is_reverse) {
                    child_x = current_main + child->constraints.margin[3];
                } else {
                    // row-reverse: main start is right edge
                    // But we calculated start_main relative to 0?
                    // If justify=flex-start (default), start_main = padding-left.
                    // For row-reverse, it should be padding-right (from right edge).
                    
                    // Let's handle coordinates explicitly.
                    // Left edge: padding[3]. Right edge: width - padding[1].
                    // If row-reverse: start is Right Edge.
                    
                    // Re-eval start_main logic for reverse?
                    // Actually, let's just use the computed offsets and flip relative to container width.
                    // X relative to content-box left: current_main - padding_left.
                    // Flipped X: content_width - (X + child_width).
                    
                    float relative_x = current_main + child->constraints.margin[3] - (node->constraints.padding[3] + node->constraints.border[3]);
                    child_x = (node->width - node->constraints.padding[1] - node->constraints.border[1]) - relative_x - child->width;
                }
                
                child->x = child_x;
                child->y = child_y;
                
                current_main += child->width + child->constraints.margin[1] + child->constraints.margin[3] + gap_actual;
            } else {
                // Column
                // Cross alignment (horizontal)
                float child_x = cross_offset + child->constraints.margin[3];
                if (align == AlignItems::CENTER) {
                    child_x = cross_offset + (line_cross_size - child->width) / 2;
                } else if (align == AlignItems::FLEX_END) {
                    child_x = cross_offset + (line_cross_size - child->width - child->constraints.margin[1]);
                }
                
                float child_y;
                if (!is_reverse) {
                    child_y = current_main + child->constraints.margin[0];
                } else {
                    float relative_y = current_main + child->constraints.margin[0] - (node->constraints.padding[0] + node->constraints.border[0]);
                    child_y = (node->height - node->constraints.padding[2] - node->constraints.border[2]) - relative_y - child->height;
                }
                
                child->x = child_x;
                child->y = child_y;
                
                current_main += child->height + child->constraints.margin[0] + child->constraints.margin[2] + gap_actual;
            }
        }
        
        cross_offset += line_cross_size; 
        // Note: gap_row/column support missing in Quadtree, using single 'gap' property for both
    }
}

void QuadtreeLayoutEngine::position_grid_children(LayoutNode* node) {
    // Note: calculate_grid_dimensions stored the position relative to content box in child->x, child->y
    // We just need to add the container's padding/border offset
    
    float start_x = node->constraints.padding[3] + node->constraints.border[3];
    float start_y = node->constraints.padding[0] + node->constraints.border[0];
    
    for (auto* child : node->children) {
        // Skip absolute positioned elements
        if (child->element->style.position == Position::ABSOLUTE || 
            child->element->style.position == Position::FIXED) continue;
            
        child->x += start_x;
        child->y += start_y;
        
        // AlignItems support is done in dimension phase implicitly by cell sizing?
        // No, in dimension phase we set child size equal to cell size (stretch).
        // If we want to support align-items (start/center/end), we should have adjusted child size there 
        // or here.
        // For now, assume stretch (default).
        // To properly support alignment, we'd need to know the cell size vs child size.
        // But we overwrote child size with cell size in calculate_grid_dimensions.
        // Correct approach would be: calculate cell size, resolve child size (if auto), then align.
        // Given existing architecture, this simplification is acceptable for now.
    }
}

// ============================================================================
// Write Results Back to Elements
// ============================================================================

void QuadtreeLayoutEngine::write_to_elements(LayoutNode* root) {
    if (!root || !root->element) return;

    // =========================================================================
    // NEW: Write to typed ResolvedLayout (source of truth)
    // =========================================================================
    root->element->layout.x = root->x;
    root->element->layout.y = root->y;
    root->element->layout.width = root->width;
    root->element->layout.height = root->height;
    root->element->layout.content_width = root->content_width;
    root->element->layout.content_height = root->content_height;

    for (int i = 0; i < 4; i++) {
        root->element->layout.padding[i] = root->constraints.padding[i];
        root->element->layout.margin[i] = root->constraints.margin[i];
        root->element->layout.border[i] = root->constraints.border[i];
        root->element->layout.radius[i] = root->element->style.border.radius[i];
    }

    // Determine layout source
    if (root->parent) {
        if (root->parent->type == LayoutNode::GRID) {
            root->element->layout.source = ResolvedLayout::Source::GRID;
        } else if (root->parent->type == LayoutNode::FLEX) {
            root->element->layout.source = ResolvedLayout::Source::FLEXBOX;
        } else {
            root->element->layout.source = ResolvedLayout::Source::FLOW;
        }
    } else {
        root->element->layout.source = ResolvedLayout::Source::EXPLICIT;
    }

    root->element->layout.is_positioned = (root->element->style.position != Position::STATIC);

    // Recursively write children
    for (auto* child : root->children) {
        write_to_elements(child);
    }
}

// ============================================================================
// Helper Functions
// ============================================================================

float QuadtreeLayoutEngine::resolve_length(const Length& length, float context_size, float font_size) {
    // Handle vw/vh separately - they need different viewport dimensions
    if (length.unit == LengthUnit::VW) {
        return length.value * viewport_width_ / 100.0f;
    }
    if (length.unit == LengthUnit::VH) {
        return length.value * viewport_height_ / 100.0f;
    }
    return length.resolve(context_size, font_size, viewport_width_);
}

float QuadtreeLayoutEngine::clamp_dimension(float value, float min_val, float max_val) {
    if (max_val >= 0) value = std::min(value, max_val);
    if (min_val >= 0) value = std::max(value, min_val);
    return value;
}

} // namespace cssbox

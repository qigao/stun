/*
 * NanoVG CSS - Flexbox Layout (Phase 4 Sprint 3)
 *
 * Implements CSS Flexbox Layout Module Level 1
 * Based on W3C specification: https://www.w3.org/TR/css-flexbox-1/
 */

#include "nanovg_css_internal.h"
#include <algorithm>
#include <cmath> 

// Forward declarations (Sprint 18 & 19)
float apply_dimension_constraints(float value, float min_value, float max_value);
void calculate_content_dimensions(
    float total_width,
    float total_height,
    const float padding[4],
    const float border[4],
    const std::string& box_sizing,
    float& content_width,
    float& content_height);

// ============================================================================
// Flexbox Property Parsing
// ============================================================================

/**
 * @brief Parse flex-grow property
 */
float parse_flex_grow(const std::string& value) {
    if (value.empty()) return 0.0f;
    try {
        return std::stof(value);
    } catch (...) {
        return 0.0f;
    }
}

/**
 * @brief Parse flex-shrink property
 */
float parse_flex_shrink(const std::string& value) {
    if (value.empty()) return 1.0f;
    try {
        return std::stof(value);
    } catch (...) {
        return 1.0f;
    }
}

/**
 * @brief Parse flex-basis property
 */
float parse_flex_basis(const std::string& value, float parent_size) {
    if (value.empty() || value == "auto") return -1.0f;  // -1 means auto

    // Parse length with unit
    if (value.find("px") != std::string::npos) {
        return std::stof(value.substr(0, value.find("px")));
    } else if (value.find("%") != std::string::npos) {
        float percent = std::stof(value.substr(0, value.find("%")));
        return (percent / 100.0f) * parent_size;
    }

    // Try as plain number (treat as px)
    try {
        return std::stof(value);
    } catch (...) {
        return -1.0f;  // auto
    }
}

/**
 * @brief Parse order property
 */
int parse_order(const std::string& value) {
    if (value.empty()) return 0;
    try {
        return std::stoi(value);
    } catch (...) {
        return 0;
    }
}

/**
 * @brief Parse gap property
 */
float parse_gap(const std::string& value) {
    if (value.empty()) return 0.0f;

    if (value.find("px") != std::string::npos) {
        return std::stof(value.substr(0, value.find("px")));
    }

    try {
        return std::stof(value);
    } catch (...) {
        return 0.0f;
    }
}

// ============================================================================
// Flexbox Layout Algorithm
// ============================================================================

/**
 * @brief Compute flexbox layout for a container
 * @param element Container element with display: flex
 * @param computed_style Container's computed style
 * @param renderer Renderer for accessing stylesheet
 */
void compute_flexbox_layout(
    NVGCSSElement* element,
    const std::map<std::string, std::string>& computed_style,
    NVGCSSRenderer* renderer)
{
    // ========================================================================
    // Step 1: Parse container properties
    // ========================================================================

    FlexContainer container;

    auto it = computed_style.find("flex-direction");
    if (it != computed_style.end()) {
        container.direction = it->second;
    }

    it = computed_style.find("flex-wrap");
    if (it != computed_style.end()) {
        container.wrap = it->second;
    }

    it = computed_style.find("justify-content");
    if (it != computed_style.end()) {
        container.justify_content = it->second;
    }

    it = computed_style.find("align-items");
    if (it != computed_style.end()) {
        container.align_items = it->second;
    }

    it = computed_style.find("align-content");
    if (it != computed_style.end()) {
        container.align_content = it->second;
    }

    it = computed_style.find("gap");
    if (it != computed_style.end()) {
        container.gap = parse_gap(it->second);
    }

    // Determine main/cross axes
    container.is_horizontal = (container.direction == "row" || container.direction == "row-reverse");
    container.is_reverse = (container.direction.find("reverse") != std::string::npos);

    float container_main_size = container.is_horizontal ? element->box.width : element->box.height;
    float container_cross_size = container.is_horizontal ? element->box.height : element->box.width;

    // ========================================================================
    // Step 2: Create flex items from children
    // ========================================================================

    std::vector<FlexItem> flex_items;

    for (auto* child : element->children) {
        if (!child->visible) continue;

        FlexItem item;
        item.element = child;

        // Get child's computed style (pass empty parent style since we already have computed_style)
        auto child_style = renderer->stylesheet->compute_style(
            child->id,
            child->type,
            child->classes,
            child->attributes,
            child->pseudo_states,
            child->inline_style,
            computed_style  // parent style for inheritance
        );

        // Parse flex properties
        auto prop_it = child_style.find("flex-grow");
        if (prop_it != child_style.end()) {
            item.flex_grow = parse_flex_grow(prop_it->second);
        }

        prop_it = child_style.find("flex-shrink");
        if (prop_it != child_style.end()) {
            item.flex_shrink = parse_flex_shrink(prop_it->second);
        }

        prop_it = child_style.find("flex-basis");
        if (prop_it != child_style.end()) {
            item.flex_basis = parse_flex_basis(prop_it->second, container_main_size);
        }

        prop_it = child_style.find("align-self");
        if (prop_it != child_style.end()) {
            item.align_self = prop_it->second;
        }

        prop_it = child_style.find("order");
        if (prop_it != child_style.end()) {
            item.order = parse_order(prop_it->second);
        }
 

        // PHASE 4 SPRINT 4: Read from explicit_style (INPUT)
        // Determine hypothetical main size
        if (item.flex_basis >= 0) {
            item.hypothetical_main_size = item.flex_basis;
        } else {
            // Use explicit size or default
            if (container.is_horizontal) {
                item.hypothetical_main_size = child->explicit_style.width >= 0 ?
                    child->explicit_style.width : 100.0f;
            } else {
                item.hypothetical_main_size = child->explicit_style.height >= 0 ?
                    child->explicit_style.height : 50.0f;
            }
        }

        // Cross size
        if (container.is_horizontal) {
            item.cross_size = child->explicit_style.height >= 0 ?
                child->explicit_style.height : 50.0f;
        } else {
            item.cross_size = child->explicit_style.width >= 0 ?
                child->explicit_style.width : 100.0f;
        }

        flex_items.push_back(item);
    }

    if (flex_items.empty()) return;

    // ========================================================================
    // Step 3: Sort by order
    // ========================================================================

    std::stable_sort(flex_items.begin(), flex_items.end(),
        [](const FlexItem& a, const FlexItem& b) {
            return a.order < b.order;
        });

    // ========================================================================
    // Step 4: Collect into flex lines
    // ========================================================================

    std::vector<FlexLine> flex_lines;

    if (container.wrap == "nowrap") {
        // Single line
        FlexLine line;
        line.items = flex_items;
        flex_lines.push_back(line);
    } else {
        // Multi-line wrapping
        FlexLine current_line;
        float current_line_size = 0;

        for (auto& item : flex_items) {
            float item_size = item.hypothetical_main_size + container.gap;

            if (!current_line.items.empty() &&
                current_line_size + item_size > container_main_size) {
                // Start new line
                flex_lines.push_back(current_line);
                current_line = FlexLine();
                current_line_size = 0;
            }

            current_line.items.push_back(item);
            current_line_size += item_size;
        }

        if (!current_line.items.empty()) {
            flex_lines.push_back(current_line);
        }
    }

    // ========================================================================
    // Step 5: Resolve flexible lengths (main axis sizing)
    // ========================================================================

    for (auto& line : flex_lines) {
        // Calculate total hypothetical size
        float total_hypothetical = 0;
        for (const auto& item : line.items) {
            total_hypothetical += item.hypothetical_main_size;
        }

        // Add gaps
        if (line.items.size() > 1) {
            total_hypothetical += container.gap * (line.items.size() - 1);
        }

        // Calculate free space
        float free_space = container_main_size - total_hypothetical;
        line.remaining_space = free_space;

   

        // Calculate total flex factors
        float total_grow = 0, total_shrink = 0;
        for (const auto& item : line.items) {
            total_grow += item.flex_grow;
            total_shrink += item.flex_shrink;
        }
 
        // Distribute free space
        if (free_space > 0 && total_grow > 0) {
            // Grow items
            for (auto& item : line.items) {
                if (item.flex_grow > 0) {
                    float ratio = item.flex_grow / total_grow;
                    item.main_size = item.hypothetical_main_size + (free_space * ratio);
                    #ifdef DEBUG_FLEXBOX
                    printf("  Item flex_grow=%.1f, ratio=%.2f, hypo=%.1f, main=%.1f",
                           item.flex_grow, ratio, item.hypothetical_main_size, item.main_size);
                    #endif
                } else {
                    item.main_size = item.hypothetical_main_size;
                }
            }
        } else if (free_space < 0 && total_shrink > 0) {
            // Shrink items
            float total_shrink_scaled = 0;
            for (auto& item : line.items) {
                item.scaled_flex_shrink = item.flex_shrink * item.hypothetical_main_size;
                total_shrink_scaled += item.scaled_flex_shrink;
            }

            for (auto& item : line.items) {
                if (item.flex_shrink > 0 && total_shrink_scaled > 0) {
                    float ratio = item.scaled_flex_shrink / total_shrink_scaled;
                    item.main_size = item.hypothetical_main_size + (free_space * ratio);
                    item.main_size = std::max(0.0f, item.main_size);  // Don't go negative
                } else {
                    item.main_size = item.hypothetical_main_size;
                }
            }
        } else {
            // No flex
            for (auto& item : line.items) {
                item.main_size = item.hypothetical_main_size;
            }
        }

        // Calculate line's main size
        line.main_size = 0;
        for (const auto& item : line.items) {
            line.main_size += item.main_size;
        }
        if (line.items.size() > 1) {
            line.main_size += container.gap * (line.items.size() - 1);
        }
    }

    // ========================================================================
    // Step 6: Determine cross-axis sizes
    // ========================================================================

    for (auto& line : flex_lines) {
        // Find maximum cross size in line
        float max_cross = 0;
        for (const auto& item : line.items) {
            max_cross = std::max(max_cross, item.cross_size);
        }

        // For single-line flexbox, line fills entire container cross axis
        if (flex_lines.size() == 1) {
            line.cross_size = container_cross_size;
        } else {
            line.cross_size = max_cross;
        }

        // Apply align-items (stretch)
        for (auto& item : line.items) {
            std::string alignment = (item.align_self != "auto")
                ? item.align_self
                : container.align_items;

            if (alignment == "stretch") {
                item.cross_size = line.cross_size;
            }
        }
    }

    // ========================================================================
    // Step 7: Main-axis alignment (justify-content)
    // ========================================================================

    for (auto& line : flex_lines) {
        float position = 0;
        float spacing = 0;
        size_t item_count = line.items.size();
        bool use_spacing = false;

        if (container.justify_content == "flex-start") {
            position = 0;
        } else if (container.justify_content == "flex-end") {
            position = container_main_size - line.main_size;
        } else if (container.justify_content == "center") {
            position = (container_main_size - line.main_size) / 2.0f;
        } else if (container.justify_content == "space-between" && item_count > 1) {
            spacing = line.remaining_space / (item_count - 1);
            use_spacing = true;
        } else if (container.justify_content == "space-around") {
            spacing = line.remaining_space / item_count;
            position = spacing / 2.0f;
            use_spacing = true;
        } else if (container.justify_content == "space-evenly") {
            spacing = line.remaining_space / (item_count + 1);
            position = spacing;
            use_spacing = true;
        }

        for (size_t i = 0; i < line.items.size(); i++) {
            auto& item = line.items[i];
            item.main_position = position;
            position += item.main_size;

            // Add spacing or gap between items (not after last item)
            if (i < line.items.size() - 1) {
                if (use_spacing) {
                    position += spacing;
                } else {
                    position += container.gap;
                }
            }
        }
    }

    // ========================================================================
    // Step 8: Cross-axis alignment (align-items)
    // ========================================================================

    for (auto& line : flex_lines) {
        for (auto& item : line.items) {
            std::string alignment = (item.align_self != "auto")
                ? item.align_self
                : container.align_items;

            if (alignment == "flex-start") {
                item.cross_position = 0;
            } else if (alignment == "flex-end") {
                item.cross_position = line.cross_size - item.cross_size;
            } else if (alignment == "center") {
                item.cross_position = (line.cross_size - item.cross_size) / 2.0f;
            } else if (alignment == "stretch" || alignment == "baseline") {
                item.cross_position = 0;
            }
        }
    }

    // ========================================================================
    // Step 9: Multi-line cross-axis alignment (align-content)
    // ========================================================================

    if (flex_lines.size() > 1) {
        float total_cross = 0;
        for (const auto& line : flex_lines) {
            total_cross += line.cross_size;
        }

        float free_space = container_cross_size - total_cross;
        float position = 0;
        float spacing = 0;

        if (container.align_content == "flex-start") {
            position = 0;
        } else if (container.align_content == "flex-end") {
            position = free_space;
        } else if (container.align_content == "center") {
            position = free_space / 2.0f;
        } else if (container.align_content == "space-between" && flex_lines.size() > 1) {
            spacing = free_space / (flex_lines.size() - 1);
        } else if (container.align_content == "space-around") {
            spacing = free_space / flex_lines.size();
            position = spacing / 2.0f;
        } else if (container.align_content == "stretch") {
            float extra = free_space / flex_lines.size();
            for (auto& line : flex_lines) {
                line.cross_size += extra;
            }
        }

        for (auto& line : flex_lines) {
            line.cross_position = position;
            position += line.cross_size + spacing;
        }
    } else if (flex_lines.size() == 1) {
        flex_lines[0].cross_position = 0;
    }

    // ========================================================================
    // Step 10: Apply final positions to elements (PHASE 4 SPRINT 4: Write to computed)
    // ========================================================================

    for (const auto& line : flex_lines) {
        for (const auto& item : line.items) {
            // Sprint 18: Apply min/max constraints
            float final_width, final_height;

            if (container.is_horizontal) {
                // Horizontal: main_size = width, cross_size = height
                final_width = apply_dimension_constraints(
                    item.main_size,
                    item.element->explicit_style.min_width,
                    item.element->explicit_style.max_width
                );
                final_height = apply_dimension_constraints(
                    item.cross_size,
                    item.element->explicit_style.min_height,
                    item.element->explicit_style.max_height
                );
            } else {
                // Vertical: main_size = height, cross_size = width
                final_width = apply_dimension_constraints(
                    item.cross_size,
                    item.element->explicit_style.min_width,
                    item.element->explicit_style.max_width
                );
                final_height = apply_dimension_constraints(
                    item.main_size,
                    item.element->explicit_style.min_height,
                    item.element->explicit_style.max_height
                );
            }

            // Sprint 19: Apply box-sizing to calculate content dimensions
            float content_width, content_height;
            float border[4] = {0.0f, 0.0f, 0.0f, 0.0f};  // TODO: border-width support

            calculate_content_dimensions(
                final_width,
                final_height,
                item.element->box.padding,
                border,
                item.element->explicit_style.box_sizing,
                content_width,
                content_height
            );

            final_width = content_width;
            final_height = content_height;

            // PHASE 4 SPRINT 4: Write to computed (OUTPUT)
            // Sprint 37: Get margin values
            float margin_left = item.element->box.margin[3];  // left
            float margin_top = item.element->box.margin[0];   // top
            float margin_right = item.element->box.margin[1];  // right
            float margin_bottom = item.element->box.margin[2]; // bottom

            if (container.is_horizontal) {
                // Horizontal layout (row)
                if (container.is_reverse) {
                    item.element->computed.x = element->box.x + element->box.width -
                                         item.main_position - item.main_size + margin_left;
                } else {
                    item.element->computed.x = element->box.x + item.main_position + margin_left;
                }
                item.element->computed.y = element->box.y + line.cross_position + item.cross_position + margin_top;
                item.element->computed.width = final_width;
                item.element->computed.height = final_height;
            } else {
                // Vertical layout (column)
                item.element->computed.x = element->box.x + line.cross_position + item.cross_position + margin_left;
                if (container.is_reverse) {
                    item.element->computed.y = element->box.y + element->box.height -
                                         item.main_position - item.main_size + margin_top;
                } else {
                    item.element->computed.y = element->box.y + item.main_position + margin_top;
                }
                item.element->computed.width = final_width;
                item.element->computed.height = final_height;
            }

            // Set content dimensions (same as outer dimensions for now)
            item.element->computed.content_width = item.element->computed.width;
            item.element->computed.content_height = item.element->computed.height;

            // Mark as computed by flexbox
            item.element->computed.source = NVGCSSComputedLayout::FLEXBOX;
            item.element->computed.is_computed = true;

            // Sync to deprecated box field for backward compatibility
            item.element->box.x = item.element->computed.x;
            item.element->box.y = item.element->computed.y;
            item.element->box.width = item.element->computed.width;
            item.element->box.height = item.element->computed.height;
        }
    }
}

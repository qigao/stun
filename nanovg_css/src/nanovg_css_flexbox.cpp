/*
 * NanoVG CSS - Flexbox Layout (Phase 4 Sprint 3)
 *
 * Implements CSS Flexbox Layout Module Level 1
 * Based on W3C specification: https://www.w3.org/TR/css-flexbox-1/
 */

#include "nanovg_css_internal.h"
#include <algorithm>
#include <cmath>
#include <cctype> 

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
 * @brief Parse dimension from style value (e.g., "80px", "10%", "auto")
 * Returns -1 for "auto", otherwise the numeric value
 */
float parse_dimension(const std::string& value, float context_size = 0) {
    if (value.empty() || value == "auto") {
        return -1.0f;
    }

    // Remove whitespace
    std::string trimmed = value;
    trimmed.erase(std::remove_if(trimmed.begin(), trimmed.end(), ::isspace), trimmed.end());

    try {
        if (trimmed.find("px") != std::string::npos) {
            return std::stof(trimmed.substr(0, trimmed.find("px")));
        } else if (trimmed.find("%") != std::string::npos) {
            float percent = std::stof(trimmed.substr(0, trimmed.find("%")));
            return (percent / 100.0f) * context_size;
        } else {
            return std::stof(trimmed);
        }
    } catch (const std::exception&) {
        // Parse error - treat as auto
        return -1.0f;
    }
}

/**
 * @brief Parse flex-grow property
 */
float parse_flex_grow(const std::string& value) {
    if (value.empty()) return 0.0f;
    try {
        return std::stof(value);
    } catch (const std::exception&) {
        // Parse error - default to 0
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
    } catch (const std::exception&) {
        // Parse error - default to 1
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
    } catch (const std::exception&) {
        // Parse error - treat as auto
        return -1.0f;
    }
}

/**
 * @brief Parse order property
 */
int parse_order(const std::string& value) {
    if (value.empty()) return 0;
    try {
        return std::stoi(value);
    } catch (const std::exception&) {
        // Parse error - default to 0
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
    } catch (const std::exception&) {
        // Parse error - default to 0
        return 0.0f;
    }
}

// ============================================================================
// Flexbox Layout Algorithm
// ============================================================================

/**
 * @brief Compute flexbox layout for a container
 * @param element Container element with display: flex (uses element->style for typed properties)
 * @param renderer Renderer for accessing stylesheet
 */
void compute_flexbox_layout(
    NVGCSSElement* element,
    NVGCSSRenderer* renderer)
{
    // ========================================================================
    // DEBUG: Check initial state
    // ========================================================================
    // printf("[FLEXBOX] === START === Container '%s'\n", element->id.c_str());
    // printf("[FLEXBOX]   Initial: is_computed=%d, height=%.1f, source=%d\n",
    //        element->computed.is_computed, element->computed.height,
    //        (int)element->computed.source);

    // ========================================================================
    // Step 1: Parse container properties
    // ========================================================================

    FlexContainer container;

    // DEBUG: Log flex container being laid out
    // printf("[FLEXBOX] Computing layout for container '%s' at (%.1f, %.1f) size (%.1f x %.1f)\n",
    //        element->id.c_str(), element->computed.x, element->computed.y,
    //        element->computed.width, element->computed.height);

    // TODO: Refactor FlexContainer to use typed enums instead of strings
    // TEMPORARY: Convert typed enums to strings for FlexContainer

    // flex-direction
    switch (element->style.flex_direction) {
        case nvgcss::FlexDirection::ROW: container.direction = "row"; break;
        case nvgcss::FlexDirection::ROW_REVERSE: container.direction = "row-reverse"; break;
        case nvgcss::FlexDirection::COLUMN: container.direction = "column"; break;
        case nvgcss::FlexDirection::COLUMN_REVERSE: container.direction = "column-reverse"; break;
    }

    // flex-wrap
    switch (element->style.flex_wrap) {
        case nvgcss::FlexWrap::NOWRAP: container.wrap = "nowrap"; break;
        case nvgcss::FlexWrap::WRAP: container.wrap = "wrap"; break;
        case nvgcss::FlexWrap::WRAP_REVERSE: container.wrap = "wrap-reverse"; break;
    }

    // justify-content
    switch (element->style.justify_content) {
        case nvgcss::JustifyContent::FLEX_START: container.justify_content = "flex-start"; break;
        case nvgcss::JustifyContent::FLEX_END: container.justify_content = "flex-end"; break;
        case nvgcss::JustifyContent::CENTER: container.justify_content = "center"; break;
        case nvgcss::JustifyContent::SPACE_BETWEEN: container.justify_content = "space-between"; break;
        case nvgcss::JustifyContent::SPACE_AROUND: container.justify_content = "space-around"; break;
        case nvgcss::JustifyContent::SPACE_EVENLY: container.justify_content = "space-evenly"; break;
    }

    // align-items
    switch (element->style.align_items) {
        case nvgcss::AlignItems::FLEX_START: container.align_items = "flex-start"; break;
        case nvgcss::AlignItems::FLEX_END: container.align_items = "flex-end"; break;
        case nvgcss::AlignItems::CENTER: container.align_items = "center"; break;
        case nvgcss::AlignItems::BASELINE: container.align_items = "baseline"; break;
        case nvgcss::AlignItems::STRETCH: container.align_items = "stretch"; break;
    }

    // align-content
    switch (element->style.align_content) {
        case nvgcss::AlignContent::FLEX_START: container.align_content = "flex-start"; break;
        case nvgcss::AlignContent::FLEX_END: container.align_content = "flex-end"; break;
        case nvgcss::AlignContent::CENTER: container.align_content = "center"; break;
        case nvgcss::AlignContent::SPACE_BETWEEN: container.align_content = "space-between"; break;
        case nvgcss::AlignContent::SPACE_AROUND: container.align_content = "space-around"; break;
        case nvgcss::AlignContent::STRETCH: container.align_content = "stretch"; break;
    }

    // gap - from typed Length property
    if (!element->style.gap.is_auto()) {
        container.gap = element->style.gap.resolve(0, 16, 800);  // Resolve to pixels
    }

    // Determine main/cross axes
    // Robust check: default to row unless "column" is explicitly specified
    // Convert to lowercase to be case-insensitive
    std::string dir = container.direction;
    std::transform(dir.begin(), dir.end(), dir.begin(), 
                   [](unsigned char c){ return std::tolower(c); });

    // DEBUG: Print direction to help diagnose test failures
    // printf("DEBUG: Flex direction raw='%s', normalized='%s'\n", container.direction.c_str(), dir.c_str());

    container.is_horizontal = (dir.find("column") == std::string::npos);
    container.is_reverse = (dir.find("reverse") != std::string::npos);
    
    // FIX: Account for padding - children should be laid out within the content area
    float padding_top = element->explicit_style.padding[0];
    float padding_right = element->explicit_style.padding[1];
    float padding_bottom = element->explicit_style.padding[2];
    float padding_left = element->explicit_style.padding[3];

    // Defensive check: if computed dims are 0, try to use explicit style
    if (element->computed.width == 0 && element->explicit_style.width > 0) {
        element->computed.width = element->explicit_style.width;
    }
    if (element->computed.height == 0 && element->explicit_style.height > 0) {
        element->computed.height = element->explicit_style.height;
    }
    
    // FIX 1: Use min-width/min-height as fallback when width/height is 0 or auto
    if (element->computed.width == 0 && element->explicit_style.min_width > 0) {
        element->computed.width = element->explicit_style.min_width;
    }
    if (element->computed.height == 0 && element->explicit_style.min_height > 0) {
        element->computed.height = element->explicit_style.min_height;
    }
    
    // FIX 2: Use viewport as last resort fallback
    if (element->computed.width == 0) {
        element->computed.width = renderer->viewport_width;
    }
    if (element->computed.height == 0) {
        element->computed.height = renderer->viewport_height;
    }

    float content_width = element->computed.width - padding_left - padding_right;
    float content_height = element->computed.height - padding_top - padding_bottom;

    float container_main_size = container.is_horizontal ? content_width : content_height;
    float container_cross_size = container.is_horizontal ? content_height : content_width;

    // ========================================================================
    // Step 2: Create flex items from children
    // ========================================================================

    std::vector<FlexItem> flex_items;

    int child_count = 0;
    NVGCSSElement** children = nvgcssGetChildren(renderer, element, &child_count);
    for (int i = 0; i < child_count; ++i) {
        NVGCSSElement* child = children[i];
        if (!child->visible) continue;

        FlexItem item;
        item.element = child;

        // DEBUG: Log each flex item being added
        // printf("[FLEXBOX]   - Child '%s' visible=%d\n", child->id.c_str(), child->visible);

        // Use child's typed properties (already computed in compute_element_layout)
        // flex-grow
        item.flex_grow = child->style.flex_grow;

        // flex-shrink
        item.flex_shrink = child->style.flex_shrink;

        // flex-basis
        if (!child->style.flex_basis.is_auto()) {
            item.flex_basis = child->style.flex_basis.resolve(container_main_size, 16, 800);
        } else {
            item.flex_basis = -1.0f;  // auto
        }

        // TODO: align-self not yet in typed system - item will use container's align_items (default "auto")
        // Need to add: AlignSelf enum in types.h, parser in lexbor_css_parser.cpp, conversion in conversion.h
        item.align_self = "auto";

        // order
        item.order = child->style.order;

        // Parse width and height from typed properties
        float child_width = -1.0f;
        float child_height = -1.0f;

        if (!child->style.width.is_auto()) {
            // FIX 3: Use viewport as fallback context when container size is 0
            float width_context = container.is_horizontal ? 
                (container_main_size > 0 ? container_main_size : renderer->viewport_width) :
                (container_cross_size > 0 ? container_cross_size : renderer->viewport_width);
            child_width = child->style.width.resolve(width_context, 16, 800);
        }

        if (!child->style.height.is_auto()) {
            // FIX 3: Use viewport as fallback context when container size is 0
            float height_context = container.is_horizontal ?
                (container_cross_size > 0 ? container_cross_size : renderer->viewport_height) :
                (container_main_size > 0 ? container_main_size : renderer->viewport_height);
            child_height = child->style.height.resolve(height_context, 16, 800);
        }

        // PHASE 4 SPRINT 4: Determine hypothetical main size
        if (item.flex_basis >= 0) {
            item.hypothetical_main_size = item.flex_basis;
        } else {
            // Use width/height from style or default
            if (container.is_horizontal) {
                item.hypothetical_main_size = child_width >= 0 ? child_width : 100.0f;
            } else {
                item.hypothetical_main_size = child_height >= 0 ? child_height : 50.0f;
            }
        }

        // Cross size
        if (container.is_horizontal) {
            item.cross_size = child_height >= 0 ? child_height : 50.0f;
            // printf("[FLEXBOX]   - Item '%s' cross_size (height) = %.1f (parsed from CSS: %.1f)\n",
            //        child->id.c_str(), item.cross_size, child_height);
        } else {
            item.cross_size = child_width >= 0 ? child_width : 100.0f;
        }

        flex_items.push_back(item);
    }

    if (flex_items.empty()) {
        // printf("[FLEXBOX] WARNING: No flex items found for container '%s'\n", element->id.c_str());
        return;
    }

    // printf("[FLEXBOX] Found %zu flex items\n", flex_items.size());

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
                    #ifdef DEBUG
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
            // printf("[FLEXBOX]   - Line item '%s' has cross_size=%.1f\n",
            //        item.element->id.c_str(), item.cross_size);
            max_cross = std::max(max_cross, item.cross_size);
        }
        // printf("[FLEXBOX]   - Line max_cross calculated as %.1f\n", max_cross);

        // For single-line flexbox, line fills entire container cross axis
        // UNLESS the container's cross-size is auto, in which case use max of items
        // Check the CSS style to see if height/width was "auto"
        bool container_cross_is_auto = false;
        if (container.is_horizontal) {
            container_cross_is_auto = element->style.height.is_auto();
        } else {
            container_cross_is_auto = element->style.width.is_auto();
        }

        // printf("[FLEXBOX]   - container_cross_is_auto=%d, flex_lines.size()=%zu, container_cross_size=%.1f\n",
        //        container_cross_is_auto, flex_lines.size(), container_cross_size);

        if (flex_lines.size() == 1 && !container_cross_is_auto) {
            line.cross_size = container_cross_size;
        } else {
            line.cross_size = max_cross;
        }
        // printf("[FLEXBOX]   - Line cross_size set to %.1f\n", line.cross_size);

        // Apply align-items (stretch)
        for (auto& item : line.items) {
            std::string alignment = (item.align_self != "auto")
                ? item.align_self
                : container.align_items;

            float old_cross = item.cross_size;
            if (alignment == "stretch") {
                item.cross_size = line.cross_size;
            }
            // printf("[FLEXBOX]   - Item '%s' alignment=%s, cross_size: %.1f -> %.1f (line.cross_size=%.1f)\n",
            //        item.element->id.c_str(), alignment.c_str(), old_cross, item.cross_size, line.cross_size);
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
                // printf("[FLEXBOX]   - Item '%s' before constraints: main_size=%.1f, cross_size=%.1f\n",
                //        item.element->id.c_str(), item.main_size, item.cross_size);
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
                // printf("[FLEXBOX]   - Item '%s' after constraints: final_width=%.1f, final_height=%.1f\n",
                //        item.element->id.c_str(), final_width, final_height);
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
            float item_content_width, item_content_height;
            float border[4] = {
                item.element->explicit_style.border_width[0],  // top
                item.element->explicit_style.border_width[1],  // right
                item.element->explicit_style.border_width[2],  // bottom
                item.element->explicit_style.border_width[3]   // left
            };

            calculate_content_dimensions(
                final_width,
                final_height,
                item.element->explicit_style.padding,
                border,
                item.element->explicit_style.box_sizing,
                item_content_width,
                item_content_height
            );

            final_width = item_content_width;
            final_height = item_content_height;

            // PHASE 4 SPRINT 4: Write to computed (OUTPUT)
            // Sprint 37: Get margin values
            float margin_left = item.element->explicit_style.margin[3];  // left
            float margin_top = item.element->explicit_style.margin[0];   // top
            float margin_right = item.element->explicit_style.margin[1];  // right
            float margin_bottom = item.element->explicit_style.margin[2]; // bottom

            if (container.is_horizontal) {
                // Horizontal layout (row)
                // FIX: Add padding offset - children should start inside the content area
                if (container.is_reverse) {
                    // Use container's content_width, not item's
                    item.element->computed.x = element->computed.x + padding_left + content_width -
                                         item.main_position - item.main_size + margin_left;
                } else {
                    item.element->computed.x = element->computed.x + padding_left + item.main_position + margin_left;
                }
                item.element->computed.y = element->computed.y + padding_top + line.cross_position + item.cross_position + margin_top;
                item.element->computed.width = final_width;
                item.element->computed.height = final_height;
            } else {
                // Vertical layout (column)
                // FIX: Add padding offset - children should start inside the content area
                item.element->computed.x = element->computed.x + padding_left + line.cross_position + item.cross_position + margin_left;
                if (container.is_reverse) {
                    // Use container's content_height, not item's
                    item.element->computed.y = element->computed.y + padding_top + content_height -
                                         item.main_position - item.main_size + margin_top;
                } else {
                    item.element->computed.y = element->computed.y + padding_top + item.main_position + margin_top;
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
        }
    }

    // ========================================================================
    // Step 8: Update container intrinsic size if height/width was auto
    // ========================================================================

    // printf("[FLEXBOX] Step 8: Updating container intrinsic size for '%s'\n", element->id.c_str());

    // FIX: Don't update intrinsic size if this container was already sized by a parent layout
    // (e.g., this flex container is a child of a grid container that already set its dimensions)
    bool already_sized_by_parent = (element->computed.is_computed &&
                                    (element->computed.source == NVGCSSComputedLayout::GRID ||
                                     element->computed.source == NVGCSSComputedLayout::FLEXBOX));

    if (already_sized_by_parent) {
        // Skip intrinsic sizing - parent layout already set dimensions
        // Mark as computed by flexbox but preserve dimensions
        element->computed.is_computed = true;
        element->computed.source = NVGCSSComputedLayout::FLEXBOX;
        return;
    }

    // Check CSS to determine if dimensions were auto (from typed properties)
    bool height_is_auto = element->style.height.is_auto();
    bool width_is_auto = element->style.width.is_auto();

    // printf("[FLEXBOX]   height_is_auto=%d, width_is_auto=%d\n", height_is_auto, width_is_auto);

    // Recalculate container cross-size if it was set to auto
    if (container.is_horizontal) {
        // For row direction: height might be auto
        if (height_is_auto) {
            float max_cross_size = 0.0f;
            for (const auto& line : flex_lines) {
                max_cross_size = std::max(max_cross_size, line.cross_size);
            }
            // Add padding
            max_cross_size += element->explicit_style.padding[0] + element->explicit_style.padding[2];
            max_cross_size += element->explicit_style.border_width[0] + element->explicit_style.border_width[2];
            // printf("[FLEXBOX]   Setting container height from %.1f to %.1f\n",
            //        element->computed.height, max_cross_size);
            element->computed.height = max_cross_size;
            element->computed.content_height = max_cross_size;
        }
    } else {
        // For column direction: height might be auto
        if (height_is_auto) {
            float total_cross_size = 0.0f;
            for (const auto& line : flex_lines) {
                total_cross_size += line.cross_size;
            }
            // Add gaps between lines
            if (flex_lines.size() > 1) {
                total_cross_size += container.gap * (flex_lines.size() - 1);
            }
            // Add padding
            total_cross_size += element->explicit_style.padding[0] + element->explicit_style.padding[2];
            total_cross_size += element->explicit_style.border_width[0] + element->explicit_style.border_width[2];
            element->computed.height = total_cross_size;
            element->computed.content_height = total_cross_size;
        }
    }

    // Similarly for width in column direction
    if (!container.is_horizontal) {
        // For column direction: width might be auto
        if (width_is_auto) {
            float max_cross_size = 0.0f;
            for (const auto& line : flex_lines) {
                max_cross_size = std::max(max_cross_size, line.cross_size);
            }
            // Add padding
            max_cross_size += element->explicit_style.padding[1] + element->explicit_style.padding[3];
            max_cross_size += element->explicit_style.border_width[1] + element->explicit_style.border_width[3];
            element->computed.width = max_cross_size;
            element->computed.content_width = max_cross_size;
        }
    } else {
        // For row direction: width might be auto
        if (width_is_auto) {
            float total_main_size = 0.0f;
            for (const auto& line : flex_lines) {
                total_main_size = std::max(total_main_size, line.main_size);
            }
            // Add padding
            total_main_size += element->explicit_style.padding[1] + element->explicit_style.padding[3];
            total_main_size += element->explicit_style.border_width[1] + element->explicit_style.border_width[3];
            element->computed.width = total_main_size;
            element->computed.content_width = total_main_size;
        }
    }

    // Mark container as computed so its intrinsic size doesn't get reset
    element->computed.is_computed = true;
    element->computed.source = NVGCSSComputedLayout::FLEXBOX;

    // RECURSIVELY compute layout for nested containers
    // If any flex item is itself a flex or grid container, compute its children
    for (const auto& line : flex_lines) {
        for (const auto& item : line.items) {
            if (!item.element) continue;

            // Check if this item is a layout container (flex or grid)
            bool is_flex = (item.element->style.display == nvgcss::Display::FLEX);
            bool is_grid = (item.element->style.display == nvgcss::Display::GRID);

            if (is_flex) {
                // Recursively compute flexbox layout for this nested container
                compute_flexbox_layout(item.element, renderer);
            } else if (is_grid) {
                // Recursively compute grid layout for this nested container
                compute_grid_layout(item.element, renderer);
            }
        }
    }
}

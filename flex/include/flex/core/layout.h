/*
 * Flex Engine - Flexbox Layout Engine
 * Implements CSS Flexbox layout algorithm
 */

#pragma once

#include "flex/core/node.h"
#include "flex/core/types.h"
#include <vector>
#include <unordered_map>
#include <cmath>
#include <algorithm>

namespace flex {

// ============================================================================
// Flexbox Layout Engine
// ============================================================================
 
enum class AlignContent {
    FlexStart,
    FlexEnd,
    Center,
    Stretch,
    SpaceBetween,
    SpaceAround,
    SpaceEvenly
};

struct FlexItem {
    Node* node = nullptr;
    float flexGrow = 0.0f;
    float flexShrink = 1.0f;
    float flexBasis = 0.0f;  // 0 = auto

    // Computed values
    float minWidth = 0.0f;
    float maxWidth = 0.0f;
    float minHeight = 0.0f;
    float maxHeight = 0.0f;

    float mainSize = 0.0f;
    float crossSize = 0.0f;
    float mainPos = 0.0f;
    float crossPos = 0.0f;

    float lineIndex = 0.0f;
};

struct FlexLine {
    std::vector<FlexItem*> items;
    float mainSize = 0.0f;
    float crossSize = 0.0f;
    float mainPos = 0.0f;
};

struct IntrinsicSize {
    float width = 0.0f;
    float height = 0.0f;
};

class FlexLayoutEngine {
public:
    FlexLayoutEngine(Node* container);

    // Layout configuration
    void set_direction(FlexDirection dir) { direction_ = dir; }
    void set_wrap(FlexWrap wrap) { wrap_ = wrap; }
    void set_justify_content(JustifyContent justify) { justify_content_ = justify; }
    void set_align_items(AlignItems align) { align_items_ = align; }
    void set_align_content(AlignContent align) { align_content_ = align; }
    void set_gap(float gap) { gap_ = gap; }
    void set_row_gap(float gap) { row_gap_ = gap; }
    void set_column_gap(float gap) { column_gap_ = gap; }

    // Run layout algorithm
    void layout(float width, float height);

private:
    Node* container_;

    // Flex container properties
    FlexDirection direction_ = FlexDirection::Row;
    FlexWrap wrap_ = FlexWrap::NoWrap;
    JustifyContent justify_content_ = JustifyContent::Start;
    AlignItems align_items_ = AlignItems::Stretch;
    AlignContent align_content_ = AlignContent::FlexStart;
    float gap_ = 0.0f;
    float row_gap_ = 0.0f;
    float column_gap_ = 0.0f;
    float container_main_size_ = 0.0f;
    float container_cross_size_ = 0.0f;

    // Helper methods
    void collect_items();
    void calculate_flex_lines(float container_main_size);
    void resolve_flexible_sizes();
    void calculate_cross_sizes();
    void distribute_free_space();
    void apply_positions();
    float get_item_main_size(const FlexItem& item);
    IntrinsicSize measure_node_intrinsic_size(Node* node, float available_width,
                                              float available_height) const;

    bool is_row() const {
        return direction_ == FlexDirection::Row || direction_ == FlexDirection::RowReverse;
    }

    bool is_column() const {
        return direction_ == FlexDirection::Column || direction_ == FlexDirection::ColumnReverse;
    }

    bool is_reversed() const {
        return direction_ == FlexDirection::RowReverse || direction_ == FlexDirection::ColumnReverse;
    }

    std::vector<FlexItem> items_;
    std::vector<FlexLine> lines_;
};

inline FlexLayoutEngine::FlexLayoutEngine(Node* container)
    : container_(container) {
    collect_items();
}

inline void FlexLayoutEngine::collect_items() {
    items_.clear();

    if (auto* group = dynamic_cast<Group*>(container_)) {
        for (const auto& child : group->children()) {
            FlexItem item;
            item.node = child;  // children() now returns vector<Node*>

            // Read flex properties from node
            item.flexGrow = child->flex_grow();
            item.flexShrink = child->flex_shrink();
            item.flexBasis = child->flex_basis();

            items_.push_back(item);
        }
    }
}

inline void FlexLayoutEngine::layout(float container_width, float container_height) {
    if (items_.empty()) return;

    if (auto* group = dynamic_cast<Group*>(container_)) {
        group->set_layout(LayoutMode::Flex);
        group->set_flex_direction(direction_);
        group->set_justify_content(justify_content_);
        group->set_align_items(align_items_);
        group->set_flex_wrap(wrap_);
        group->set_gap(gap_);
        group->set_layout_size(container_width, container_height);
        group->perform_layout();
        return;
    }

    // Reset
    lines_.clear();

    // Determine container main and cross sizes
    float container_main_size = is_row() ? container_width : container_height;
    float container_cross_size = is_row() ? container_height : container_width;
    container_main_size_ = container_main_size;
    container_cross_size_ = container_cross_size;

    // Collect items and calculate flex lines
    collect_items();
    calculate_flex_lines(container_main_size);

    // Run flexbox algorithm
    resolve_flexible_sizes();
    calculate_cross_sizes();
    distribute_free_space();
    apply_positions();
}

inline void FlexLayoutEngine::calculate_flex_lines(float container_main_size) {
    FlexLine current_line;
    float line_main_size = 0.0f;

    for (auto& item : items_) {
        float item_main_size = 0.0f;

        const auto intrinsic = measure_node_intrinsic_size(item.node, is_row() ? container_main_size : 0.0f,
                                                           is_row() ? 0.0f : container_main_size);
        item_main_size = is_row() ? intrinsic.width : intrinsic.height;
        item.mainSize = item_main_size;

        // Check if item fits on current line
        const float projected =
            current_line.items.empty() ? item_main_size : line_main_size + gap_ + item_main_size;
        if (wrap_ == FlexWrap::NoWrap || projected <= container_main_size) {
            current_line.items.push_back(&item);
            line_main_size = projected;
        } else {
            // New line
            if (!current_line.items.empty()) {
                current_line.mainSize = line_main_size;
                lines_.push_back(current_line);
                current_line = FlexLine();
                line_main_size = 0.0f;
            }
            current_line.items.push_back(&item);
            line_main_size = item_main_size;
        }
    }

    // Add last line
    if (!current_line.items.empty()) {
        current_line.mainSize = line_main_size;
        lines_.push_back(current_line);
    }
}

inline void FlexLayoutEngine::resolve_flexible_sizes() {
    for (auto& line : lines_) {
        if (line.items.empty()) continue;

        float total_flex_grow = 0.0f;
        float total_flex_shrink = 0.0f;
        float allocated_size = 0.0f;
        float total_gaps = line.items.size() > 1 ? gap_ * static_cast<float>(line.items.size() - 1) : 0.0f;

        // Calculate totals
        for (auto* item : line.items) {
            allocated_size += get_item_main_size(*item);
            total_flex_grow += item->flexGrow;
            total_flex_shrink += item->flexShrink;
        }

        float free_space = container_main_size_ - allocated_size - total_gaps;

        // Distribute free space based on flex properties
        if (free_space > 0.0f && total_flex_grow > 0.0f) {
            // Growing
            for (auto* item : line.items) {
                if (item->flexGrow > 0.0f) {
                    float grow_amount = (free_space * item->flexGrow) / total_flex_grow;
                    item->mainSize += grow_amount;
                }
            }
        } else if (free_space < 0.0f && total_flex_shrink > 0.0f) {
            // Shrinking
            for (auto* item : line.items) {
                if (item->flexShrink > 0.0f) {
                    float shrink_amount = (-free_space * item->flexShrink) / total_flex_shrink;
                    item->mainSize -= shrink_amount;
                    item->mainSize = (std::max)(item->mainSize, 0.0f);
                }
            }
        }
    }
}

inline void FlexLayoutEngine::calculate_cross_sizes() {
    for (auto& line : lines_) {
        line.crossSize = 0.0f;

        for (auto* item : line.items) {
            float cross_size = 0.0f;
            const auto intrinsic =
                measure_node_intrinsic_size(item->node, is_row() ? item->mainSize : line.crossSize,
                                            is_row() ? line.crossSize : item->mainSize);
            cross_size = is_row() ? intrinsic.height : intrinsic.width;

            item->crossSize = cross_size;
            line.crossSize = (std::max)(line.crossSize, cross_size);
        }
    }
}

inline void FlexLayoutEngine::distribute_free_space() {
    for (auto& line : lines_) {
        float total_main_size = 0.0f;
        for (auto* item : line.items) {
            total_main_size += item->mainSize;
        }

        float free_space = line.mainSize - total_main_size;

        // Distribute based on justifyContent
        float offset = 0.0f;
        float gap = line.items.size() > 1 ? gap_ : 0.0f;
        if (line.items.size() > 1) {
            total_main_size += gap_ * static_cast<float>(line.items.size() - 1);
        }
        free_space = container_main_size_ - total_main_size;

        if (justify_content_ == JustifyContent::Start) {
            offset = 0.0f;
        } else if (justify_content_ == JustifyContent::End) {
            offset = free_space;
        } else if (justify_content_ == JustifyContent::Center) {
            offset = free_space / 2.0f;
        } else if (justify_content_ == JustifyContent::SpaceBetween && line.items.size() > 1) {
            offset = 0.0f;
            gap = free_space / (line.items.size() - 1);
        } else if (justify_content_ == JustifyContent::SpaceAround && line.items.size() > 0) {
            offset = free_space / (2.0f * line.items.size());
            gap = free_space / line.items.size();
        }

        // Position items
        float current_main = offset;
        for (size_t i = 0; i < line.items.size(); ++i) {
            auto* item = line.items[i];
            item->mainPos = current_main;
            current_main += item->mainSize;
            if (i + 1 < line.items.size()) {
                current_main += gap;
            }
        }
    }
}

inline void FlexLayoutEngine::apply_positions() {
    float cross_offset = 0.0f;
    const float line_gap = is_row() ? row_gap_ : column_gap_;
    for (auto& line : lines_) {
        for (auto* item : line.items) {
            float cross_pos = cross_offset;

            // Calculate cross position based on alignItems
            switch (align_items_) {
                case AlignItems::Start:
                    break;
                case AlignItems::End:
                    cross_pos += line.crossSize - item->crossSize;
                    break;
                case AlignItems::Center:
                    cross_pos += (line.crossSize - item->crossSize) / 2.0f;
                    break;
                case AlignItems::Stretch:
                    item->crossSize = line.crossSize;
                    break;
            }

            // Apply position based on direction
            if (is_row()) {
                if (is_reversed()) {
                    item->node->set_x(container_main_size_ - item->mainPos - item->mainSize);
                } else {
                    item->node->set_x(item->mainPos);
                }
                item->node->set_y(cross_pos);
            } else {
                item->node->set_x(cross_pos);
                if (is_reversed()) {
                    item->node->set_y(container_main_size_ - item->mainPos - item->mainSize);
                } else {
                    item->node->set_y(item->mainPos);
                }
            }
        }

        // Move to next line
        cross_offset += line.crossSize + line_gap;
    }
}

inline float FlexLayoutEngine::get_item_main_size(const FlexItem& item) {
    if (item.mainSize > 0.0f) {
        return item.mainSize;
    }
    const auto intrinsic = measure_node_intrinsic_size(item.node, 0.0f, 0.0f);
    return is_row() ? intrinsic.width : intrinsic.height;
}

inline IntrinsicSize FlexLayoutEngine::measure_node_intrinsic_size(Node* node,
                                                                   float available_width,
                                                                   float available_height) const {
    if (!node) {
        return {};
    }

    const float resolved_width = node->width_is_percent() && available_width > 0.0f
                                     ? available_width * (node->layout_width() / 100.0f)
                                     : node->layout_width();
    const float resolved_height = node->height_is_percent() && available_height > 0.0f
                                      ? available_height * (node->layout_height() / 100.0f)
                                      : node->layout_height();

    IntrinsicSize size{resolved_width > 0.0f ? resolved_width : 0.0f,
                       resolved_height > 0.0f ? resolved_height : 0.0f};
    if (size.width > 0.0f && size.height > 0.0f) {
        return size;
    }

    auto* group = dynamic_cast<Group*>(node);
    if (group && !group->children().empty() && group->layout() != LayoutMode::None) {
        if (size.width > 0.0f) {
            group->set_layout_width(size.width);
        }
        if (size.height > 0.0f) {
            group->set_layout_height(size.height);
        }
        group->perform_layout();
        group->mark_dirty(DirtyFlags::Bounds);
        const Bounds bounds = group->bounds();
        if (size.width <= 0.0f) {
            size.width = bounds.width;
        }
        if (size.height <= 0.0f) {
            size.height = bounds.height;
        }
        return size;
    }

    const Bounds bounds = node->bounds();
    if (size.width <= 0.0f) {
        size.width = bounds.width;
    }
    if (size.height <= 0.0f) {
        size.height = bounds.height;
    }
    return size;
}

} // namespace flex

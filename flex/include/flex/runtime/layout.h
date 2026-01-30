/*
 * Flex Engine - Flexbox Layout Engine
 * Implements CSS Flexbox layout algorithm
 */

#pragma once

#include "flex/runtime/node.h"
#include "flex/runtime/types.h"
#include <vector>
#include <unordered_map>
#include <cmath>

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

    // Helper methods
    void collect_items();
    void calculate_flex_lines(float container_main_size);
    void resolve_flexible_sizes();
    void calculate_cross_sizes();
    void distribute_free_space();
    void apply_positions();
    float get_item_main_size(const FlexItem& item);

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

    // Reset
    lines_.clear();

    // Determine container main and cross sizes
    float container_main_size = is_row() ? container_width : container_height;
    float container_cross_size = is_row() ? container_height : container_width;

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

        // Calculate base size (for now, use layout_width/height or bounds)
        if (is_row()) {
            item_main_size = item.node->layout_width() > 0 ?
                item.node->layout_width() : item.node->bounds().width;
        } else {
            item_main_size = item.node->layout_height() > 0 ?
                item.node->layout_height() : item.node->bounds().height;
        }

        // Check if item fits on current line
        if (wrap_ == FlexWrap::NoWrap || line_main_size + item_main_size <= container_main_size) {
            current_line.items.push_back(&item);
            line_main_size += item_main_size + gap_;
        } else {
            // New line
            if (!current_line.items.empty()) {
                current_line.mainSize = line_main_size;
                lines_.push_back(current_line);
                current_line = FlexLine();
                line_main_size = 0.0f;
            }
            current_line.items.push_back(&item);
            line_main_size += item_main_size + gap_;
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

        // Calculate totals
        for (auto* item : line.items) {
            allocated_size += get_item_main_size(*item);
            total_flex_grow += item->flexGrow;
            total_flex_shrink += item->flexShrink;
        }

        float free_space = line.mainSize - allocated_size;

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

            if (is_row()) {
                cross_size = item->node->layout_height() > 0 ?
                    item->node->layout_height() : item->node->bounds().height;
            } else {
                cross_size = item->node->layout_width() > 0 ?
                    item->node->layout_width() : item->node->bounds().width;
            }

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
        for (size_t i = 0; i < line.items.size(); ++i) {
            auto* item = line.items[i];
            item->mainPos = offset + i * (item->mainSize + gap);
        }
    }
}

inline void FlexLayoutEngine::apply_positions() {
    for (auto& line : lines_) {
        for (auto* item : line.items) {
            float main_pos = item->mainPos;
            float cross_pos = 0.0f;

            // Calculate cross position based on alignItems
            switch (align_items_) {
                case AlignItems::Start:
                    cross_pos = 0.0f;
                    break;
                case AlignItems::End:
                    cross_pos = line.crossSize - item->crossSize;
                    break;
                case AlignItems::Center:
                    cross_pos = (line.crossSize - item->crossSize) / 2.0f;
                    break;
                case AlignItems::Stretch:
                    cross_pos = 0.0f;
                    item->crossSize = line.crossSize;
                    break;
            }

            // Apply position based on direction
            if (is_row()) {
                if (is_reversed()) {
                    item->node->set_x(container_->bounds().width - item->mainPos - item->mainSize);
                } else {
                    item->node->set_x(item->mainPos);
                }
                item->node->set_y(cross_pos);
            } else {
                item->node->set_x(cross_pos);
                if (is_reversed()) {
                    item->node->set_y(container_->bounds().height - item->mainPos - item->mainSize);
                } else {
                    item->node->set_y(item->mainPos);
                }
            }
        }

        // Move to next line
        line.mainPos += line.crossSize + column_gap_;
    }
}

inline float FlexLayoutEngine::get_item_main_size(const FlexItem& item) {
    if (is_row()) {
        return item.node->layout_width() > 0 ?
            item.node->layout_width() : item.node->bounds().width;
    } else {
        return item.node->layout_height() > 0 ?
            item.node->layout_height() : item.node->bounds().height;
    }
}

} // namespace flex

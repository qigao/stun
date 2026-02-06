#pragma once

namespace flex::modules::flexmaid {

// Layout modes for node internal elements
enum class NodeLayoutMode { VerticalStack, C4Grid };

struct NodeLayout {
    float padding = 10.0f;
    
    struct Pos { float x, y; };
    Pos type_label, icon, name;
    
    void compute(NodeLayoutMode mode, float rect_x, float rect_y, float width, float height) {
        float cx = rect_x + width / 2;
        float left = rect_x + padding;
        float right = rect_x + width - padding - 12;
        
        if (mode == NodeLayoutMode::VerticalStack) {
            // Centered vertical: type(top) -> icon(middle) -> name(bottom)
            type_label = {cx, rect_y + 15};
            icon = {cx, rect_y + height / 2};
            name = {cx, rect_y + height - 15};
        } else {
            // C4 Grid: Row1=type(left), Row2=name(left)+icon(right)
            type_label = {left, rect_y + 15};
            name = {left, rect_y + height / 2 + 5};
            icon = {right, rect_y + height / 2 + 5};
        }
    }
};

} // namespace flex::modules::flexmaid

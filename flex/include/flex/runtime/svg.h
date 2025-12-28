/*
 * Flex Engine - SVG Node
 *
 * Vector graphic node for rendering SVG content.
 */

#pragma once

#include "flex/runtime/node.h"
#include "flex/runtime/types.h"
#include "flex/runtime/allocator.h"
#include <string>

namespace flex {

class Svg : public Node {
public:
    using Ptr = Svg*;

    Svg() = default;
    ~Svg() override = default;

    static Ptr create(ArenaAllocator& arena) { return arena.create<Svg>(); }

    NodeType type() const override { return NodeType::Svg; }
    const char* type_name() const override { return "Svg"; }

    // Source (file path)
    const std::string& src() const { return src_; }
    void set_src(const std::string& src) { src_ = src; data_.clear(); }

    // Inline SVG data
    const std::string& data() const { return data_; }
    void set_data(const std::string& data) { data_ = data; src_.clear(); }

    // Dimensions (0 = use natural size)
    float width() const { return width_; }
    void set_width(float w) { width_ = w; }

    float height() const { return height_; }
    void set_height(float h) { height_ = h; }

    // Rendering
    void render(Renderer& renderer) override;

private:
    std::string src_;   // File path
    std::string data_;  // Inline SVG content
    float width_ = 0;
    float height_ = 0;
};

} // namespace flex

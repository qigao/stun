/*
 * Flex Engine - Text Node
 *
 * Typography node for rendering text with decoration and measurement support.
 */

#pragma once

#include "flex/runtime/node.h"
#include "flex/runtime/types.h"
#include <string>

namespace flex {

enum class TextAlign {
    Left,
    Center,
    Right,
};

enum class FontWeight {
    Normal,
    Bold,
};

enum class FontStyle {
    Normal,
    Italic,
};

enum class TextDecoration {
    None,
    Underline,
    Strikethrough,
    Overline,
};

enum class TextOverflow {
    Visible,    // Show all text (default)
    Clip,       // Clip at boundary
    Ellipsis,   // Show "..." for overflow
};

class Text : public Node {
public:
    using Ptr = std::shared_ptr<Text>;

    Text() = default;
    ~Text() override = default;

    static Ptr create() { return std::make_shared<Text>(); }

    NodeType type() const override { return NodeType::Text; }
    const char* type_name() const override { return "Text"; }

    // Content
    const std::string& content() const { return content_; }
    void set_content(const std::string& content) { content_ = content; invalidate_measurement(); mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds); }

    // Font
    const std::string& font_family() const { return font_family_; }
    void set_font_family(const std::string& family) { font_family_ = family; invalidate_measurement(); mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds); }

    float font_size() const { return font_size_; }
    void set_font_size(float size) { font_size_ = size; invalidate_measurement(); mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds); }

    FontWeight font_weight() const { return font_weight_; }
    void set_font_weight(FontWeight weight) { font_weight_ = weight; invalidate_measurement(); mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds); }

    FontStyle font_style() const { return font_style_; }
    void set_font_style(FontStyle style) { font_style_ = style; invalidate_measurement(); mark_dirty(DirtyFlags::Content | DirtyFlags::Visual); }

    // Text Decoration
    TextDecoration text_decoration() const { return text_decoration_; }
    void set_text_decoration(TextDecoration decoration) { text_decoration_ = decoration; mark_dirty(DirtyFlags::Visual); }

    // Layout
    TextAlign text_align() const { return text_align_; }
    void set_text_align(TextAlign align) { text_align_ = align; mark_dirty(DirtyFlags::Visual); }

    float line_height() const { return line_height_; }
    void set_line_height(float height) { line_height_ = height; invalidate_measurement(); mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds); }

    float max_width() const { return max_width_; }
    void set_max_width(float width) { max_width_ = width; invalidate_measurement(); mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds); }

    float letter_spacing() const { return letter_spacing_; }
    void set_letter_spacing(float spacing) { letter_spacing_ = spacing; invalidate_measurement(); mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds); }

    // Overflow
    TextOverflow text_overflow() const { return text_overflow_; }
    void set_text_overflow(TextOverflow overflow) { text_overflow_ = overflow; mark_dirty(DirtyFlags::Visual); }

    // Color
    const Color& color() const { return color_; }
    void set_color(const Color& color) { color_ = color; mark_dirty(DirtyFlags::Visual); }
    void set_color(uint32_t rgba) { color_ = Color::from_rgba32(rgba); mark_dirty(DirtyFlags::Visual); }

    // -------------------------------------------
    // Text Measurement
    // -------------------------------------------

    // Get measured text bounds (width x height)
    // Returns cached measurement, recalculates if invalidated
    Bounds compute_bounds() const override;

    // Get measured text width (approximate, based on font metrics)
    float measured_width() const;

    // Get measured text height (based on font size and line height)
    float measured_height() const;

    // Force recalculation of text metrics
    void invalidate_measurement() { measurement_valid_ = false; }

    // -------------------------------------------
    // Rendering
    // -------------------------------------------

    void render(Renderer& renderer) override;

private:
    void update_measurement() const;

    std::string content_;
    std::string font_family_ = "sans-serif";
    float font_size_ = 16.0f;
    FontWeight font_weight_ = FontWeight::Normal;
    FontStyle font_style_ = FontStyle::Normal;
    TextDecoration text_decoration_ = TextDecoration::None;
    TextAlign text_align_ = TextAlign::Left;
    float line_height_ = 1.2f;
    float max_width_ = 0;  // 0 = no limit
    float letter_spacing_ = 0;  // Extra space between characters
    TextOverflow text_overflow_ = TextOverflow::Visible;
    Color color_ = {0, 0, 0, 1};  // Default black

    // Cached measurement
    mutable bool measurement_valid_ = false;
    mutable float measured_width_ = 0;
    mutable float measured_height_ = 0;
};

} // namespace flex

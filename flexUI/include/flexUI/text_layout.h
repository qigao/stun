#ifndef FLEXUI_TEXT_LAYOUT_H
#define FLEXUI_TEXT_LAYOUT_H

#include "computed_style.h"
#include "render_command.h"
#include <string>
#include <vector>

namespace flexUI {

enum class TextVerticalAlign {
  Top,
  Middle,
  Bottom,
};

struct TextLayoutLine {
  std::string text;
  float x = 0.0f;
  float baseline_y = 0.0f;
  float width = 0.0f;
  float justify_spacing = 0.0f;
};

struct TextLayoutBlock {
  std::vector<TextLayoutLine> lines;
  Color color{0.0f, 0.0f, 0.0f, 1.0f};
  Color decoration_color{0.0f, 0.0f, 0.0f, 1.0f};
  std::vector<TextShadow> text_shadows;
  std::string font_family = "Arial";
  float font_size = 16.0f;
  bool bold = false;
  bool tabular_nums = false;
  bool overline = false;
  bool underline = false;
  bool line_through = false;
  BorderStyle decoration_style = BorderStyle::Solid;
  float letter_spacing = 0.0f;
  float word_spacing = 0.0f;
  float text_indent = 0.0f;
  float tab_size = 8.0f;
  float decoration_thickness = 1.0f;
  float underline_offset = 0.0f;
  float line_through_offset = 0.0f;
  float line_height = 0.0f;
};

struct EditableTextMetrics {
  std::string font_family = "Arial";
  float font_size = 16.0f;
  float line_height = 0.0f;
  float char_width = 0.0f;
  float padding_left = 0.0f;
  float padding_right = 0.0f;
  float padding_top = 0.0f;
  Direction direction = Direction::Ltr;
  Color text_color{0.0f, 0.0f, 0.0f, 1.0f};
  Color placeholder_color{0.5f, 0.5f, 0.5f, 1.0f};
};

std::string transform_text_for_layout(const ComputedStyle* style,
                                      const std::string& text);
float approximate_text_width(const ComputedStyle* style, const std::string& text);
float approximate_segmented_text_width(const ComputedStyle* style,
                                       const std::string& text);
float emit_segmented_text_line(RenderCommandList& commands, const ComputedStyle* style,
                               const std::string& text, float x, float baseline_y,
                               const Color& color, bool bold = false);
float resolve_line_height(const ComputedStyle* style);
float resolve_line_height_with_default_multiplier(const ComputedStyle* style,
                                                  float default_multiplier);
float resolve_line_text_top(float line_top, const EditableTextMetrics& metrics);
TextVerticalAlign resolve_text_vertical_align(const ComputedStyle* style,
                                              TextVerticalAlign fallback =
                                                  TextVerticalAlign::Middle);
Direction resolve_text_direction_for_content(const ComputedStyle* style,
                                             const std::string& text);
EditableTextMetrics resolve_editable_text_metrics(
    const ComputedStyle* style, float font_size_fallback,
    Symbol text_color_var, const Color& default_text_color,
    Symbol placeholder_color_var, const Color& default_placeholder_color,
    float default_line_height_multiplier = 1.5f);
TextLayoutBlock layout_text_block(const ComputedStyle* style, const std::string& text,
                                  float x, float y, float width, float height,
                                  const Color& color,
                                  TextVerticalAlign vertical_align =
                                      TextVerticalAlign::Middle);
void emit_text_block(RenderCommandList& commands, const TextLayoutBlock& block);

} // namespace flexUI

#endif // FLEXUI_TEXT_LAYOUT_H

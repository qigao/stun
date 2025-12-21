/*
 * tvgbox2 - LabelWidget Implementation
 */

#include <tvgbox2/widgets/label_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <tvgbox2/renderer.h>
#include <tvgbox2/text_util.h>
#include <thorvg.h>
#include <algorithm>
#include <sstream>

namespace tvgbox2 {

// ============================================================================
// 构造函数
// ============================================================================

LabelWidget::LabelWidget(const std::string& text)
    : text_(text) {}

// ============================================================================
// Widget 接口实现
// ============================================================================

void LabelWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  render_text(scene, elem);
}

bool LabelWidget::handle_event(const Event& event, Element& elem) {
  // Label 无交互
  return false;
}

void LabelWidget::update(float delta_ms, Element& elem) {
  // Label 无动画
}

// ============================================================================
// 渲染辅助
// ============================================================================

void LabelWidget::render_text(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  // 使用 Element.text_content 或 Widget 内部的 text_
  std::string display_text = !elem.text_content.empty() ? elem.text_content : text_;
  if (display_text.empty()) return;

  // 处理文本（溢出、换行等）
  std::string processed_text = process_text(display_text, elem);
  if (processed_text.empty()) return;

  // 文本颜色
  Color text_color = style->get_variable_color("--text-color", style->text_color);

  // 文本对齐
  std::string text_align = style->get_variable("--text-align", "left");
  std::string vertical_align = style->get_variable("--vertical-align", "top");

  // 基础位置
  float base_x = style->padding[3];  // left padding
  float base_y = style->padding[0];  // top padding

  // 计算总宽度（用于居中/右对齐）
  float total_width = 0;
  auto segments = segment_text(processed_text);
  for (const auto& seg : segments) {
    // 简化：假设等宽字体，emoji 按 1.2x 宽度
    float char_width = style->font_size * 0.6f;
    if (seg.type == TextSegmentType::Emoji) {
      // Emoji 通常是方形，宽度约等于字体大小
      size_t emoji_count = 0;
      size_t pos = 0;
      while (pos < seg.text.size()) {
        utf8_decode(seg.text, pos);
        emoji_count++;
      }
      total_width += emoji_count * style->font_size;
    } else {
      total_width += seg.text.size() * char_width;
    }
  }

  // 水平对齐
  if (text_align == "center") {
    base_x = (elem.width() - total_width) / 2;
  } else if (text_align == "right") {
    base_x = elem.width() - total_width - style->padding[1];
  }

  // 垂直对齐
  if (vertical_align == "middle") {
    base_y = (elem.height() + style->font_size) / 2;
  } else if (vertical_align == "bottom") {
    base_y = elem.height() - style->padding[2];
  } else {
    base_y += style->font_size;  // top: baseline
  }

  // 渲染每个文本段
  float current_x = base_x;
  for (const auto& seg : segments) {
    auto text_shape = tvg::Text::gen();

    if (seg.type == TextSegmentType::Emoji) {
      // 使用 emoji 字体
      text_shape->font(get_emoji_font_name());
    } else {
      // 使用主字体
      text_shape->font(style->font_family.c_str());
    }

    text_shape->size(style->font_size);
    text_shape->text(seg.text.c_str());
    text_shape->fill(text_color.r, text_color.g, text_color.b);
    text_shape->opacity(text_color.a);
    text_shape->translate(current_x, base_y);

    scene->push(std::move(text_shape));

    // 更新 x 位置
    if (seg.type == TextSegmentType::Emoji) {
      size_t emoji_count = 0;
      size_t pos = 0;
      while (pos < seg.text.size()) {
        utf8_decode(seg.text, pos);
        emoji_count++;
      }
      current_x += emoji_count * style->font_size;
    } else {
      current_x += seg.text.size() * style->font_size * 0.6f;
    }
  }
}

// ============================================================================
// 文本处理
// ============================================================================

std::string LabelWidget::process_text(const std::string& text, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return text;

  std::string result = text;

  // 1. white-space 处理
  std::string white_space = style->get_variable("--white-space", "normal");

  if (white_space == "nowrap") {
    // 移除换行符
    result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
  }

  // 2. text-overflow 处理
  std::string text_overflow = style->get_variable("--text-overflow", "clip");

  if (text_overflow == "ellipsis") {
    // 计算可用宽度
    float max_width = elem.width() - style->padding[1] - style->padding[3];
    result = apply_ellipsis(result, max_width, style->font_size);
  }

  // 3. max-lines / line-clamp 处理
  std::string max_lines_str = style->get_variable("--max-lines", "");
  if (max_lines_str.empty()) {
    max_lines_str = style->get_variable("--line-clamp", "");
  }

  if (!max_lines_str.empty()) {
    int max_lines = std::stoi(max_lines_str);
    if (max_lines > 0) {
      // 简化版：按换行符分割
      std::istringstream ss(result);
      std::string line;
      std::string limited_text;
      int line_count = 0;

      while (std::getline(ss, line) && line_count < max_lines) {
        if (line_count > 0) limited_text += "\n";
        limited_text += line;
        line_count++;
      }

      // 如果还有更多行，添加省略号
      if (std::getline(ss, line)) {
        limited_text += "...";
      }

      result = limited_text;
    }
  }

  return result;
}

std::string LabelWidget::apply_ellipsis(const std::string& text, float max_width, float font_size) {
  // 简化版：假设等宽字体
  float char_width = font_size * 0.6f;
  size_t max_chars = static_cast<size_t>(max_width / char_width);

  if (text.size() <= max_chars) {
    return text;
  }

  // 截断并添加省略号
  if (max_chars < 3) return "...";
  return text.substr(0, max_chars - 3) + "...";
}

} // namespace tvgbox2

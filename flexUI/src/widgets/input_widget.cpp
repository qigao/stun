/*
 * flexUI - InputWidget Implementation
 *
 * Uses stb_textedit for robust text editing with undo/redo support.
 */

#include <flexUI/widgets/input_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/renderer.h>
#include <flexUI/text_util.h>
#include <algorithm>
#include <cmath>

namespace flexUI {

// ============================================================================
// 构造函数
// ============================================================================

InputWidget::InputWidget(const std::string& placeholder, bool password)
    : placeholder_(placeholder), password_(password) {
  textedit_ = std::make_unique<TextEdit>();
}

// ============================================================================
// TextEdit 初始化（延迟）
// ============================================================================

void InputWidget::ensure_textedit_init(const Element& elem) {
  if (textedit_initialized_) return;

  auto* style = elem.computed_style;
  float char_width = style ? style->font_size * 0.6f : 10.0f;
  float line_height = style ? style->font_size * 1.2f : 16.0f;

  char_width_ = char_width;
  textedit_->init(&text_, char_width, line_height, false);
  textedit_initialized_ = true;
}

// ============================================================================
// 文本访问
// ============================================================================

void InputWidget::set_text(const std::string& text) {
  if (text_ != text) {
    text_ = text;
    // Re-init textedit with new text
    textedit_initialized_ = false;
    dirty_ = true;
  }
}

void InputWidget::set_cursor_pos(size_t pos) {
  if (textedit_) {
    textedit_->set_cursor(static_cast<int>(pos));
  }
  dirty_ = true;
}

std::string InputWidget::selected_text() const {
  if (textedit_) {
    return textedit_->selected_text();
  }
  return "";
}

void InputWidget::select_all() {
  if (textedit_) {
    textedit_->select_all();
  }
  dirty_ = true;
}

void InputWidget::clear_selection() {
  if (textedit_) {
    textedit_->clear_selection();
  }
  dirty_ = true;
}

// ============================================================================
// Widget 接口实现
// ============================================================================

void InputWidget::render(const Element& elem, Renderer& renderer) {
  // Ensure textedit is initialized for cursor/selection queries
  const_cast<InputWidget*>(this)->ensure_textedit_init(elem);

  auto& r = renderer.flex();

  render_background(r, elem);

  if (has_selection()) {
    render_selection(r, elem);
  }

  render_text(r, elem);

  // 只有聚焦时才显示光标
  if (elem.has_state("focus") && cursor_visible_) {
    render_cursor(r, elem);
  }
}

bool InputWidget::handle_event(const Event& event, Element& elem) {
  ensure_textedit_init(elem);

  switch (event.type) {
    case EventType::KeyDown:
      return handle_key_down(event, elem);

    case EventType::TextInput:
      return handle_text_input(event, elem);

    case EventType::MouseDown:
      return handle_mouse_down(event, elem);

    case EventType::MouseMove:
      return handle_mouse_move(event, elem);

    case EventType::MouseUp:
      return handle_mouse_up(event, elem);

    default:
      return false;
  }
}

void InputWidget::update(float delta_ms, Element& elem) {
  // 光标闪烁动画（每 500ms 切换一次）
  if (elem.has_state("focus")) {
    cursor_blink_time_ += delta_ms;
    if (cursor_blink_time_ >= 500) {
      cursor_blink_time_ = 0;
      cursor_visible_ = !cursor_visible_;
      elem.mark_paint_dirty();
    }
  } else {
    cursor_visible_ = false;
  }
}

// ============================================================================
// 渲染辅助
// ============================================================================

void InputWidget::render_background(flex::Renderer& r, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  float radius = style->border_radius[0];  // 使用第一个值（四角相同）

  // 背景色：CSS 变量 --input-bg 或默认
  Color bg_color = style->get_variable_color("--input-bg", {0.94f, 0.94f, 0.94f, 1.0f});

  r.draw_rect(0, 0, elem.width(), elem.height(), radius,
              Paint::solid(bg_color), Paint::none(), 0);

  // 边框：CSS 变量 --input-border 或默认
  Color border_color = style->get_variable_color("--input-border", {0.78f, 0.78f, 0.78f, 1.0f});

  r.draw_rect(0, 0, elem.width(), elem.height(), radius,
              Paint::none(), Paint::solid(border_color), 1);
}

void InputWidget::render_text(flex::Renderer& r, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  std::string display = display_text();

  // 文本位置计算
  float text_x = style->padding[3];  // left padding
  float text_y = (elem.height() - style->font_size) / 2;  // 垂直居中

  // 如果为空，显示 placeholder
  if (display.empty() && !placeholder_.empty()) {
    Color placeholder_color = style->get_variable_color("--input-placeholder", {0.63f, 0.63f, 0.63f, 1.0f});

    // Placeholder 也支持 emoji
    auto segments = segment_text(placeholder_);
    float current_x = text_x;

    for (const auto& seg : segments) {
      std::string font_name;
      if (seg.type == TextSegmentType::Emoji) {
        font_name = get_emoji_font_name();
      } else {
        font_name = style->font_family;
      }

      r.draw_text(seg.text, current_x, text_y, font_name, style->font_size, false, placeholder_color);

      // 更新位置
      if (seg.type == TextSegmentType::Emoji) {
        size_t count = 0;
        size_t pos = 0;
        while (pos < seg.text.size()) {
          utf8_decode(seg.text, pos);
          count++;
        }
        current_x += count * style->font_size;
      } else {
        current_x += seg.text.size() * style->font_size * 0.6f;
      }
    }
    return;
  }

  // 正常文本 - 支持 emoji
  if (!display.empty()) {
    Color text_color = style->get_variable_color("--input-text", {0.0f, 0.0f, 0.0f, 1.0f});

    auto segments = segment_text(display);
    float current_x = text_x;

    for (const auto& seg : segments) {
      std::string font_name;
      if (seg.type == TextSegmentType::Emoji) {
        font_name = get_emoji_font_name();
      } else {
        font_name = style->font_family;
      }

      r.draw_text(seg.text, current_x, text_y, font_name, style->font_size, false, text_color);

      // 更新位置
      if (seg.type == TextSegmentType::Emoji) {
        size_t count = 0;
        size_t pos = 0;
        while (pos < seg.text.size()) {
          utf8_decode(seg.text, pos);
          count++;
        }
        current_x += count * style->font_size;
      } else {
        current_x += seg.text.size() * style->font_size * 0.6f;
      }
    }
  }
}

void InputWidget::render_cursor(flex::Renderer& r, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  int cursor_pos = textedit_ ? textedit_->cursor() : 0;
  float cursor_x = index_to_x(cursor_pos, elem);
  float cursor_h = style->font_size;
  float cursor_y = (elem.height() - cursor_h) / 2;

  Color cursor_color = style->get_variable_color("--input-cursor", {0.0f, 0.0f, 0.0f, 1.0f});

  r.draw_rect(cursor_x, cursor_y, 1, cursor_h, 0, Paint::solid(cursor_color), Paint::none(), 0);
}

void InputWidget::render_selection(flex::Renderer& r, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style || !textedit_) return;

  int start = textedit_->selection_start();
  int end = textedit_->selection_end();

  float start_x = index_to_x(start, elem);
  float end_x = index_to_x(end, elem);

  Color selection_bg = style->get_variable_color("--input-selection-bg", {0.39f, 0.59f, 1.0f, 0.5f});

  r.draw_rect(start_x, 0, end_x - start_x, elem.height(), 0, Paint::solid(selection_bg), Paint::none(), 0);
}

// ============================================================================
// 事件处理
// ============================================================================

bool InputWidget::handle_key_down(const Event& event, Element& elem) {
  bool shift = (event.mods & static_cast<int>(KeyMod::Shift)) != 0;
  bool ctrl = (event.mods & static_cast<int>(KeyMod::Control)) != 0;

  // Try stb_textedit key handling first
  int stb_key = map_key_to_stb(static_cast<int>(event.key), shift, ctrl);
  if (stb_key != 0 && textedit_) {
    textedit_->key(static_cast<int>(event.key), shift, ctrl);

    // 重置光标闪烁
    cursor_blink_time_ = 0;
    cursor_visible_ = true;

    // Notify change
    if (change_callback_) change_callback_(text_);

    elem.mark_paint_dirty();
    return true;
  }

  // Handle special keys not in stb_textedit
  switch (event.key) {
    case KeyCode::A:
      if (ctrl && textedit_) {
        textedit_->select_all();
        elem.mark_paint_dirty();
        return true;
      }
      break;

    case KeyCode::Z:
      if (ctrl && textedit_) {
        if (shift) {
          textedit_->redo();
        } else {
          textedit_->undo();
        }
        if (change_callback_) change_callback_(text_);
        elem.mark_paint_dirty();
        return true;
      }
      break;

    case KeyCode::Y:
      if (ctrl && textedit_) {
        textedit_->redo();
        if (change_callback_) change_callback_(text_);
        elem.mark_paint_dirty();
        return true;
      }
      break;

    // TODO: Ctrl+C/X/V 需要剪贴板支持

    default:
      break;
  }

  return false;
}

bool InputWidget::handle_text_input(const Event& event, Element& elem) {
  if (textedit_) {
    textedit_->insert_text(event.text);

    // 重置光标闪烁
    cursor_blink_time_ = 0;
    cursor_visible_ = true;

    if (change_callback_) change_callback_(text_);

    elem.mark_paint_dirty();
    return true;
  }
  return false;
}

bool InputWidget::handle_mouse_down(const Event& event, Element& elem) {
  if (!textedit_) return false;

  auto* style = elem.computed_style;
  float text_x = style ? style->padding[3] : 0;

  // 点击位置（相对于文本区域）
  float relative_x = event.x - elem.absolute_x() - text_x;
  float relative_y = event.y - elem.absolute_y();

  textedit_->click(relative_x, relative_y);
  is_dragging_ = true;

  // 重置光标闪烁
  cursor_blink_time_ = 0;
  cursor_visible_ = true;

  elem.mark_paint_dirty();
  return true;
}

bool InputWidget::handle_mouse_move(const Event& event, Element& elem) {
  if (is_dragging_ && textedit_) {
    auto* style = elem.computed_style;
    float text_x = style ? style->padding[3] : 0;

    float relative_x = event.x - elem.absolute_x() - text_x;
    float relative_y = event.y - elem.absolute_y();

    textedit_->drag(relative_x, relative_y);
    elem.mark_paint_dirty();
    return true;
  }
  return false;
}

bool InputWidget::handle_mouse_up(const Event& event, Element& elem) {
  is_dragging_ = false;
  return false;
}

// ============================================================================
// 坐标转换
// ============================================================================

size_t InputWidget::x_to_index(float x, const Element& elem) const {
  auto* style = elem.computed_style;
  if (!style) return 0;

  float text_x = style->padding[3];  // left padding
  float relative_x = x - text_x;

  if (relative_x <= 0) return 0;

  std::string display = display_text();
  if (display.empty()) return 0;

  // 简化版：假设等宽字体
  float char_width = char_width_ > 0 ? char_width_ : style->font_size * 0.6f;
  size_t index = static_cast<size_t>(relative_x / char_width + 0.5f);
  return std::min(index, display.size());
}

float InputWidget::index_to_x(size_t index, const Element& elem) const {
  auto* style = elem.computed_style;
  if (!style) return 0;

  float text_x = style->padding[3];  // left padding

  if (index == 0) return text_x;

  std::string display = display_text();
  if (display.empty()) return text_x;

  // 计算到 index 位置的宽度，考虑 emoji
  float x = text_x;
  size_t byte_pos = 0;
  size_t char_idx = 0;

  while (byte_pos < display.size() && char_idx < index) {
    size_t char_start = byte_pos;
    uint32_t cp = utf8_decode(display, byte_pos);

    if (is_emoji(cp)) {
      // Emoji 宽度 = font_size
      x += style->font_size;
    } else {
      // Regular char 宽度
      x += style->font_size * 0.6f;
    }
    char_idx++;
  }

  return x;
}

std::string InputWidget::display_text() const {
  if (!password_) return text_;

  // 密码模式：返回相同长度的 *
  return std::string(text_.size(), '*');
}

} // namespace flexUI

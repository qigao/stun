/*
 * tvgbox2 - TextAreaWidget Implementation
 */

#include <tvgbox2/widgets/textarea_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <tvgbox2/renderer.h>
#include <thorvg.h>
#include <algorithm>
#include <sstream>

namespace tvgbox2 {

// ============================================================================
// 构造函数
// ============================================================================

TextAreaWidget::TextAreaWidget(const std::string& text, const std::string& placeholder)
    : text_(text), placeholder_(placeholder) {
  split_lines();
}

// ============================================================================
// 状态访问
// ============================================================================

void TextAreaWidget::set_text(const std::string& text) {
  if (text_ == text) return;
  text_ = text;
  split_lines();

  cursor_pos_ = std::min(cursor_pos_, static_cast<int>(text_.size()));
  selection_start_ = -1;
  selection_end_ = -1;
  dirty_ = true;
}

void TextAreaWidget::set_cursor_position(int pos) {
  cursor_pos_ = std::max(0, std::min(pos, static_cast<int>(text_.size())));
  cursor_blink_time_ = 0.0f;
  cursor_visible_ = true;
  dirty_ = true;
}

// ============================================================================
// Widget 接口实现
// ============================================================================

void TextAreaWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  render_background(scene, elem);

  if (text_.empty() && !placeholder_.empty() && !elem.has_state("focus")) {
    render_placeholder(scene, elem);
  } else {
    render_text_lines(scene, elem);

    if (selection_start_ != -1 && selection_end_ != -1) {
      render_selection(scene, elem);
    }

    if (elem.has_state("focus") && cursor_visible_) {
      render_cursor(scene, elem);
    }
  }
}

bool TextAreaWidget::handle_event(const Event& event, Element& elem) {
  if (disabled_ || elem.has_state("disabled")) {
    return false;
  }

  switch (event.type) {
    case EventType::MouseDown:
      return handle_mouse_down(event, elem);

    case EventType::KeyDown:
      return handle_key_down(event, elem);

    case EventType::TextInput:
      if (!readonly_ && !elem.has_state("readonly")) {
        return handle_text_input(event, elem);
      }
      return false;

    default:
      return false;
  }
}

void TextAreaWidget::update(float delta_ms, Element& elem) {
  if (elem.has_state("focus")) {
    update_cursor_blink(delta_ms);
  }
}

// ============================================================================
// 文本处理
// ============================================================================

void TextAreaWidget::split_lines() {
  lines_.clear();

  std::stringstream ss(text_);
  std::string line;

  while (std::getline(ss, line, '\n')) {
    lines_.push_back(line);
  }

  // 如果文本以换行符结尾，添加空行
  if (!text_.empty() && text_.back() == '\n') {
    lines_.push_back("");
  }

  // 至少有一行（即使为空）
  if (lines_.empty()) {
    lines_.push_back("");
  }
}

void TextAreaWidget::rebuild_text() {
  std::string new_text;
  for (size_t i = 0; i < lines_.size(); i++) {
    new_text += lines_[i];
    if (i < lines_.size() - 1) {
      new_text += '\n';
    }
  }
  text_ = new_text;
}

int TextAreaWidget::get_line_from_cursor() const {
  int pos = 0;
  for (size_t i = 0; i < lines_.size(); i++) {
    int line_end = pos + static_cast<int>(lines_[i].size());
    if (cursor_pos_ <= line_end) {
      return static_cast<int>(i);
    }
    pos = line_end + 1;  // +1 for '\n'
  }
  return static_cast<int>(lines_.size()) - 1;
}

int TextAreaWidget::get_column_from_cursor() const {
  int pos = 0;
  int line = get_line_from_cursor();

  for (int i = 0; i < line; i++) {
    pos += static_cast<int>(lines_[i].size()) + 1;  // +1 for '\n'
  }

  return cursor_pos_ - pos;
}

void TextAreaWidget::move_cursor_to_line_column(int line, int col) {
  line = std::max(0, std::min(line, static_cast<int>(lines_.size()) - 1));
  col = std::max(0, std::min(col, static_cast<int>(lines_[line].size())));

  int pos = 0;
  for (int i = 0; i < line; i++) {
    pos += static_cast<int>(lines_[i].size()) + 1;
  }
  pos += col;

  set_cursor_position(pos);
}

// ============================================================================
// 渲染辅助
// ============================================================================

void TextAreaWidget::render_background(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  Color bg_color = style->get_variable_color("--textarea-bg", {255, 255, 255, 255});
  Color border_color = style->get_variable_color("--textarea-border", {200, 200, 200, 255});

  // 背景
  auto rect = tvg::Shape::gen();
  rect->appendRect(0, 0, elem.width(), elem.height(), 4, 4);
  rect->fill(bg_color.r, bg_color.g, bg_color.b, bg_color.a);
  scene->push(std::move(rect));

  // 边框
  auto border = tvg::Shape::gen();
  border->appendRect(0, 0, elem.width(), elem.height(), 4, 4);
  border->strokeFill(border_color.r, border_color.g, border_color.b, border_color.a);
  border->strokeWidth(elem.has_state("focus") ? 2.0f : 1.0f);
  scene->push(std::move(border));
}

void TextAreaWidget::render_text_lines(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  Color text_color = style->get_variable_color("--textarea-text", {0, 0, 0, 255});
  float line_height_multiplier = style->get_variable_float("--line-height", 1.5f);
  float line_height = style->font_size * line_height_multiplier;

  float padding_left = style->padding[3];
  float padding_top = style->padding[0];
  float y = padding_top + style->font_size;

  for (const auto& line : lines_) {
    if (y - scroll_offset_ > elem.height()) break;  // 超出可见区域
    if (y - scroll_offset_ + line_height < 0) {     // 在可见区域之前
      y += line_height;
      continue;
    }

    if (!line.empty()) {
      auto text_shape = tvg::Text::gen();
      text_shape->font(style->font_family.c_str());
      text_shape->size(style->font_size);
      text_shape->text(line.c_str());
      text_shape->fill(text_color.r, text_color.g, text_color.b);
      text_shape->opacity(text_color.a);
      text_shape->translate(padding_left, y - scroll_offset_);

      scene->push(std::move(text_shape));
    }

    y += line_height;
  }
}

void TextAreaWidget::render_placeholder(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  Color placeholder_color = style->get_variable_color("--textarea-placeholder", {160, 160, 160, 255});

  auto text_shape = tvg::Text::gen();
  text_shape->font(style->font_family.c_str());
  text_shape->size(style->font_size);
  text_shape->text(placeholder_.c_str());
  text_shape->fill(placeholder_color.r, placeholder_color.g, placeholder_color.b);
  text_shape->opacity(placeholder_color.a);

  float x = style->padding[3];
  float y = style->padding[0] + style->font_size;
  text_shape->translate(x, y);

  scene->push(std::move(text_shape));
}

void TextAreaWidget::render_selection(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  Color sel_color = style->get_variable_color("--textarea-selection-bg", {100, 149, 237, 128});
  float line_height = style->font_size * style->get_variable_float("--line-height", 1.5f);
  float padding_left = style->padding[3];
  float padding_top = style->padding[0];

  int sel_start = std::min(selection_start_, selection_end_);
  int sel_end = std::max(selection_start_, selection_end_);

  // 简化版：绘制单个矩形（完整实现需要多行选择）
  float char_width = style->font_size * 0.6f;  // 近似
  float x = padding_left + sel_start * char_width;
  float y = padding_top;
  float width = (sel_end - sel_start) * char_width;
  float height = line_height;

  auto sel_rect = tvg::Shape::gen();
  sel_rect->appendRect(x, y - scroll_offset_, width, height);
  sel_rect->fill(sel_color.r, sel_color.g, sel_color.b, sel_color.a);
  scene->push(std::move(sel_rect));
}

void TextAreaWidget::render_cursor(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  Color cursor_color = style->get_variable_color("--textarea-cursor", {0, 0, 0, 255});
  float line_height = style->font_size * style->get_variable_float("--line-height", 1.5f);

  int line = get_line_from_cursor();
  int col = get_column_from_cursor();

  float char_width = style->font_size * 0.6f;
  float padding_left = style->padding[3];
  float padding_top = style->padding[0];

  float x = padding_left + col * char_width;
  float y = padding_top + line * line_height;

  auto cursor_line = tvg::Shape::gen();
  cursor_line->appendRect(x, y - scroll_offset_, 2, line_height);
  cursor_line->fill(cursor_color.r, cursor_color.g, cursor_color.b, cursor_color.a);
  scene->push(std::move(cursor_line));
}

// ============================================================================
// 事件处理
// ============================================================================

bool TextAreaWidget::handle_mouse_down(const Event& event, Element& elem) {
  elem.add_state("focus");

  // 简化：将点击转换为光标位置（完整实现需要精确计算）
  auto* style = elem.computed_style;
  if (!style) return true;

  float char_width = style->font_size * 0.6f;
  float line_height = style->font_size * style->get_variable_float("--line-height", 1.5f);
  float padding_left = style->padding[3];
  float padding_top = style->padding[0];

  int line = static_cast<int>((event.y - padding_top + scroll_offset_) / line_height);
  int col = static_cast<int>((event.x - padding_left) / char_width);

  move_cursor_to_line_column(line, col);
  selection_start_ = -1;
  selection_end_ = -1;

  return true;
}

bool TextAreaWidget::handle_key_down(const Event& event, Element& elem) {
  bool shift = (event.mods & static_cast<int>(KeyMod::Shift)) != 0;
  bool ctrl = (event.mods & static_cast<int>(KeyMod::Control)) != 0;

  if (readonly_ || elem.has_state("readonly")) {
    // 只读模式只允许导航
    switch (event.key) {
      case KeyCode::Left:
      case KeyCode::Right:
      case KeyCode::Up:
      case KeyCode::Down:
      case KeyCode::Home:
      case KeyCode::End:
        break;
      default:
        return false;
    }
  }

  switch (event.key) {
    case KeyCode::Backspace:
      if (selection_start_ != -1) {
        delete_selection();
      } else {
        delete_char_before_cursor();
      }
      return true;

    case KeyCode::Delete:
      if (selection_start_ != -1) {
        delete_selection();
      } else {
        delete_char_after_cursor();
      }
      return true;

    case KeyCode::Left:
      if (cursor_pos_ > 0) {
        set_cursor_position(cursor_pos_ - 1);
      }
      if (!shift) {
        selection_start_ = -1;
        selection_end_ = -1;
      }
      return true;

    case KeyCode::Right:
      if (cursor_pos_ < static_cast<int>(text_.size())) {
        set_cursor_position(cursor_pos_ + 1);
      }
      if (!shift) {
        selection_start_ = -1;
        selection_end_ = -1;
      }
      return true;

    case KeyCode::Up: {
      int line = get_line_from_cursor();
      int col = get_column_from_cursor();
      if (line > 0) {
        move_cursor_to_line_column(line - 1, col);
      }
      if (!shift) {
        selection_start_ = -1;
        selection_end_ = -1;
      }
      return true;
    }

    case KeyCode::Down: {
      int line = get_line_from_cursor();
      int col = get_column_from_cursor();
      if (line < static_cast<int>(lines_.size()) - 1) {
        move_cursor_to_line_column(line + 1, col);
      }
      if (!shift) {
        selection_start_ = -1;
        selection_end_ = -1;
      }
      return true;
    }

    case KeyCode::Home: {
      int line = get_line_from_cursor();
      move_cursor_to_line_column(line, 0);
      if (!shift) {
        selection_start_ = -1;
        selection_end_ = -1;
      }
      return true;
    }

    case KeyCode::End: {
      int line = get_line_from_cursor();
      move_cursor_to_line_column(line, static_cast<int>(lines_[line].size()));
      if (!shift) {
        selection_start_ = -1;
        selection_end_ = -1;
      }
      return true;
    }

    case KeyCode::Enter:
      if (!readonly_ && !elem.has_state("readonly")) {
        insert_text("\n");
      }
      return true;

    case KeyCode::A:
      if (ctrl) {
        selection_start_ = 0;
        selection_end_ = static_cast<int>(text_.size());
        dirty_ = true;
        return true;
      }
      break;

    default:
      break;
  }

  return false;
}

bool TextAreaWidget::handle_text_input(const Event& event, Element& elem) {
  insert_text(event.text);
  return true;
}

void TextAreaWidget::insert_text(const std::string& str) {
  if (selection_start_ != -1) {
    delete_selection();
  }

  text_.insert(cursor_pos_, str);
  cursor_pos_ += static_cast<int>(str.size());
  split_lines();

  cursor_blink_time_ = 0.0f;
  cursor_visible_ = true;
  dirty_ = true;

  if (change_callback_) {
    change_callback_(text_);
  }
}

void TextAreaWidget::delete_selection() {
  int start = std::min(selection_start_, selection_end_);
  int end = std::max(selection_start_, selection_end_);

  text_.erase(start, end - start);
  cursor_pos_ = start;
  selection_start_ = -1;
  selection_end_ = -1;
  split_lines();

  dirty_ = true;

  if (change_callback_) {
    change_callback_(text_);
  }
}

void TextAreaWidget::delete_char_before_cursor() {
  if (cursor_pos_ > 0) {
    text_.erase(cursor_pos_ - 1, 1);
    cursor_pos_--;
    split_lines();
    dirty_ = true;

    if (change_callback_) {
      change_callback_(text_);
    }
  }
}

void TextAreaWidget::delete_char_after_cursor() {
  if (cursor_pos_ < static_cast<int>(text_.size())) {
    text_.erase(cursor_pos_, 1);
    split_lines();
    dirty_ = true;

    if (change_callback_) {
      change_callback_(text_);
    }
  }
}

// ============================================================================
// 动画更新
// ============================================================================

void TextAreaWidget::update_cursor_blink(float delta_ms) {
  cursor_blink_time_ += delta_ms;

  if (cursor_blink_time_ >= 530.0f) {
    cursor_visible_ = !cursor_visible_;
    cursor_blink_time_ = 0.0f;
    dirty_ = true;
  }
}

} // namespace tvgbox2

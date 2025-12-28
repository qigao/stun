/*
 * flexUI - InputWidget
 *
 * 文本输入控件 - 使用 flex::Renderer 渲染
 */

#ifndef FLEXUI_INPUT_WIDGET_H
#define FLEXUI_INPUT_WIDGET_H

#include "../textedit.h"
#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <functional>
#include <memory>
#include <string>

namespace flexUI {

/**
 * InputWidget - 文本输入框
 *
 * CSS 变量：
 *   --input-bg: 背景色
 *   --input-text: 文字颜色
 *   --input-border: 边框颜色
 *   --input-placeholder: placeholder 颜色
 *   --input-selection-bg: 选中背景色
 *   --input-cursor: 光标颜色
 */
class InputWidget : public Widget {
public:
  explicit InputWidget(const std::string &placeholder = "", bool password = false);

  void render(const Element &elem, Renderer &renderer) override;
  bool handle_event(const Event &event, Element &elem) override;
  void update(float delta_ms, Element &elem) override;
  const char *type_name() const override { return "InputWidget"; }
  bool wants_mouse_capture() const override { return is_dragging_; }

  // Text access
  const std::string &text() const { return text_; }
  void set_text(const std::string &text);

  const std::string &placeholder() const { return placeholder_; }
  void set_placeholder(const std::string &text) { placeholder_ = text; dirty_ = true; }

  bool is_password() const { return password_; }
  void set_password(bool enabled) { password_ = enabled; dirty_ = true; }

  // Selection and cursor
  size_t cursor_pos() const { return textedit_ ? textedit_->cursor() : 0; }
  void set_cursor_pos(size_t pos);

  bool has_selection() const { return textedit_ && textedit_->has_selection(); }
  std::string selected_text() const;
  void select_all();
  void clear_selection();

  // Callback
  using ChangeCallback = std::function<void(const std::string &text)>;
  void set_change_callback(ChangeCallback callback) { change_callback_ = callback; }

private:
  void render_background(flex::Renderer& r, const Element &elem);
  void render_text(flex::Renderer& r, const Element &elem);
  void render_cursor(flex::Renderer& r, const Element &elem);
  void render_selection(flex::Renderer& r, const Element &elem);

  bool handle_key_down(const Event &event, Element &elem);
  bool handle_text_input(const Event &event, Element &elem);
  bool handle_mouse_down(const Event &event, Element &elem);
  bool handle_mouse_move(const Event &event, Element &elem);
  bool handle_mouse_up(const Event &event, Element &elem);

  size_t x_to_index(float x, const Element &elem) const;
  float index_to_x(size_t index, const Element &elem) const;
  std::string display_text() const;
  void ensure_textedit_init(const Element &elem);

  std::string text_;
  std::string placeholder_;
  bool password_ = false;

  std::unique_ptr<TextEdit> textedit_;
  bool textedit_initialized_ = false;
  float char_width_ = 0;

  ChangeCallback change_callback_;
  bool is_dragging_ = false;

  float cursor_blink_time_ = 0;
  bool cursor_visible_ = true;
};

} // namespace flexUI

#endif // FLEXUI_INPUT_WIDGET_H

/*
 * flexUI - TextAreaWidget
 *
 * 多行文本输入 - 使用 flex::Renderer 渲染
 */

#ifndef FLEXUI_TEXTAREA_WIDGET_H
#define FLEXUI_TEXTAREA_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <string>
#include <functional>
#include <vector>

namespace flexUI {

/**
 * TextAreaWidget - 多行文本输入
 *
 * CSS 变量支持：
 *   --textarea-bg: "r,g,b,a"
 *   --textarea-text: "r,g,b,a"
 *   --textarea-border: "r,g,b,a"
 *   --textarea-placeholder: "r,g,b,a"
 *   --textarea-cursor: "r,g,b,a"
 *   --textarea-selection-bg: "r,g,b,a"
 */
class TextAreaWidget : public Widget {
public:
  explicit TextAreaWidget(const std::string& text = "",
                          const std::string& placeholder = "");

  void render(const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "TextAreaWidget"; }

  const std::string& text() const { return text_; }
  void set_text(const std::string& text);

  const std::string& placeholder() const { return placeholder_; }
  void set_placeholder(const std::string& placeholder) { placeholder_ = placeholder; dirty_ = true; }

  bool is_readonly() const { return readonly_; }
  void set_readonly(bool readonly) { readonly_ = readonly; dirty_ = true; }

  bool is_disabled() const { return disabled_; }
  void set_disabled(bool disabled) { disabled_ = disabled; dirty_ = true; }

  int cursor_position() const { return cursor_pos_; }
  void set_cursor_position(int pos);

  using ChangeCallback = std::function<void(const std::string& text)>;
  void set_change_callback(ChangeCallback callback) { change_callback_ = callback; }

private:
  void split_lines();
  void rebuild_text();

  int get_line_from_cursor() const;
  int get_column_from_cursor() const;
  void move_cursor_to_line_column(int line, int col);

  void render_background(flex::Renderer& r, const Element& elem);
  void render_text_lines(flex::Renderer& r, const Element& elem);
  void render_placeholder(flex::Renderer& r, const Element& elem);
  void render_selection(flex::Renderer& r, const Element& elem);
  void render_cursor(flex::Renderer& r, const Element& elem);

  bool handle_mouse_down(const Event& event, Element& elem);
  bool handle_key_down(const Event& event, Element& elem);
  bool handle_text_input(const Event& event, Element& elem);

  void insert_text(const std::string& str);
  void delete_selection();
  void delete_char_before_cursor();
  void delete_char_after_cursor();

  void update_cursor_blink(float delta_ms);

  std::string text_;
  std::string placeholder_;
  std::vector<std::string> lines_;

  int cursor_pos_ = 0;
  int selection_start_ = -1;
  int selection_end_ = -1;

  bool readonly_ = false;
  bool disabled_ = false;

  float cursor_blink_time_ = 0.0f;
  bool cursor_visible_ = true;

  float scroll_offset_ = 0.0f;

  ChangeCallback change_callback_;
};

} // namespace flexUI

#endif // FLEXUI_TEXTAREA_WIDGET_H

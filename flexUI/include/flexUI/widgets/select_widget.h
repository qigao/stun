/*
 * flexUI - SelectWidget
 *
 * 下拉选择控件 - 使用 Group/Shape 渲染
 */

#ifndef FLEXUI_SELECT_WIDGET_H
#define FLEXUI_SELECT_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <string>
#include <functional>
#include <vector>

namespace flexUI {

/**
 * SelectWidget - 下拉选择器
 *
 * CSS 变量支持：
 *   --select-bg: "r,g,b,a"                 // 背景色
 *   --select-text: "r,g,b,a"               // 文字颜色
 *   --select-border: "r,g,b,a"             // 边框颜色
 *   --select-arrow: "r,g,b,a"              // 箭头颜色
 *   --dropdown-bg: "r,g,b,a"               // 下拉列表背景
 *   --dropdown-item-hover: "r,g,b,a"       // 选项悬停背景
 *   --dropdown-item-selected: "r,g,b,a"    // 选中选项背景
 *   --dropdown-max-height: "200"           // 下拉列表最大高度
 *   --item-height: "32"                    // 单个选项高度
 *   --transition-duration: "200"           // 展开/收起动画时长
 */
class SelectWidget : public Widget {
public:
  explicit SelectWidget(const std::vector<std::string>& options = {},
                       int selected_index = -1);

  void render(const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "SelectWidget"; }
  bool wants_mouse_capture() const override { return expanded_; }

  // State access
  const std::vector<std::string>& options() const { return options_; }
  void set_options(const std::vector<std::string>& options);

  int selected_index() const { return selected_index_; }
  void set_selected_index(int index);

  std::string selected_value() const {
    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(options_.size())) {
      return options_[selected_index_];
    }
    return "";
  }

  bool is_expanded() const { return expanded_; }
  void set_expanded(bool expanded);

  bool is_disabled() const { return disabled_; }
  void set_disabled(bool disabled) { disabled_ = disabled; dirty_ = true; }

  // Callback
  using ChangeCallback = std::function<void(int index, const std::string& value)>;
  void set_change_callback(ChangeCallback callback) { change_callback_ = callback; }

private:
  void render_select_box(flex::Renderer& r, const Element& elem);
  void render_selected_text(flex::Renderer& r, const Element& elem);
  void render_arrow(flex::Renderer& r, const Element& elem);
  void render_dropdown(flex::Renderer& r, const Element& elem);

  bool handle_mouse_down(const Event& event, Element& elem);
  bool handle_mouse_move(const Event& event, Element& elem);
  bool handle_key_down(const Event& event, Element& elem);

  void select_option(int index, Element& elem);
  void toggle_dropdown(Element& elem);
  void update_dropdown_animation(float delta_ms);

  std::vector<std::string> options_;
  int selected_index_ = -1;
  int hovered_index_ = -1;

  bool expanded_ = false;
  bool disabled_ = false;

  float dropdown_height_ = 0.0f;
  float target_dropdown_height_ = 0.0f;

  ChangeCallback change_callback_;
};

} // namespace flexUI

#endif // FLEXUI_SELECT_WIDGET_H

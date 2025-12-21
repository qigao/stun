/*
 * tvgbox2 - SelectWidget
 *
 * 下拉选择控件 - CSS 控制样式
 */

#ifndef TVGBOX2_SELECT_WIDGET_H
#define TVGBOX2_SELECT_WIDGET_H

#include "../widget.h"
#include <string>
#include <functional>
#include <vector>

namespace tvgbox2 {

/**
 * SelectWidget - 下拉选择器
 *
 * 设计理念：从列表中选择单个选项
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
 *
 * 伪状态支持：
 *   :expanded  - 展开状态
 *   :focus     - 获得焦点
 *   :disabled  - 禁用状态
 *   :hover     - 鼠标悬停
 *
 * 示例用法（CSS）：
 *   select {
 *     --select-bg: 255,255,255,255;
 *     --select-border: 200,200,200,255;
 *     --item-height: 32;
 *   }
 *   select:expanded {
 *     --select-border: 59,130,246,255;
 *   }
 */
class SelectWidget : public Widget {
public:
  /**
   * 构造函数
   *
   * @param options 选项列表
   * @param selected_index 初始选中索引（-1 表示无选择）
   */
  explicit SelectWidget(const std::vector<std::string>& options = {},
                       int selected_index = -1);

  // ========================================================================
  // Widget 接口实现
  // ========================================================================

  void render(tvg::Scene* scene, const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "SelectWidget"; }

  // ========================================================================
  // 状态访问
  // ========================================================================

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

  // ========================================================================
  // 回调
  // ========================================================================

  using ChangeCallback = std::function<void(int index, const std::string& value)>;
  void set_change_callback(ChangeCallback callback) { change_callback_ = callback; }

private:
  // ========== 渲染辅助 ==========

  void render_select_box(tvg::Scene* scene, const Element& elem);
  void render_selected_text(tvg::Scene* scene, const Element& elem);
  void render_arrow(tvg::Scene* scene, const Element& elem);
  void render_dropdown(tvg::Scene* scene, const Element& elem);

  // ========== 事件处理 ==========

  bool handle_mouse_down(const Event& event, Element& elem);
  bool handle_mouse_move(const Event& event, Element& elem);
  bool handle_key_down(const Event& event, Element& elem);

  void select_option(int index, Element& elem);
  void toggle_dropdown(Element& elem);

  // ========== 动画更新 ==========

  void update_dropdown_animation(float delta_ms);

  // ========== 状态 ==========

  std::vector<std::string> options_;
  int selected_index_ = -1;
  int hovered_index_ = -1;      // 鼠标悬停的选项

  bool expanded_ = false;
  bool disabled_ = false;

  // 下拉列表动画
  float dropdown_height_ = 0.0f;
  float target_dropdown_height_ = 0.0f;

  // 回调
  ChangeCallback change_callback_;
};

} // namespace tvgbox2

#endif // TVGBOX2_SELECT_WIDGET_H

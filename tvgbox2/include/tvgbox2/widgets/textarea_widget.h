/*
 * tvgbox2 - TextAreaWidget
 *
 * 多行文本输入控件 - CSS 控制样式
 */

#ifndef TVGBOX2_TEXTAREA_WIDGET_H
#define TVGBOX2_TEXTAREA_WIDGET_H

#include "../widget.h"
#include <string>
#include <functional>
#include <vector>

namespace tvgbox2 {

/**
 * TextAreaWidget - 多行文本输入
 *
 * 设计理念：支持多行编辑、换行、滚动
 *
 * CSS 变量支持：
 *   --textarea-bg: "r,g,b,a"               // 背景色
 *   --textarea-text: "r,g,b,a"             // 文字颜色
 *   --textarea-border: "r,g,b,a"           // 边框颜色
 *   --textarea-placeholder: "r,g,b,a"      // 占位符颜色
 *   --textarea-cursor: "r,g,b,a"           // 光标颜色
 *   --textarea-selection-bg: "r,g,b,a"     // 选择背景色
 *   --min-rows: "3"                        // 最小行数
 *   --max-rows: "0"                        // 最大行数（0=无限）
 *   --line-height: "1.5"                   // 行高倍数
 *   --wrap-mode: "wrap"                    // wrap/nowrap
 *
 * 伪状态支持：
 *   :focus     - 获得焦点
 *   :disabled  - 禁用状态
 *   :readonly  - 只读状态
 *
 * 示例用法（CSS）：
 *   textarea {
 *     --textarea-bg: 255,255,255,255;
 *     --textarea-text: 0,0,0,255;
 *     --min-rows: 3;
 *     --line-height: 1.5;
 *   }
 *   textarea:focus {
 *     --textarea-border: 59,130,246,255;
 *   }
 */
class TextAreaWidget : public Widget {
public:
  /**
   * 构造函数
   *
   * @param text 初始文本内容
   * @param placeholder 占位符文本
   */
  explicit TextAreaWidget(const std::string& text = "",
                          const std::string& placeholder = "");

  // ========================================================================
  // Widget 接口实现
  // ========================================================================

  void render(tvg::Scene* scene, const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "TextAreaWidget"; }

  // ========================================================================
  // 状态访问
  // ========================================================================

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

  // ========================================================================
  // 回调
  // ========================================================================

  using ChangeCallback = std::function<void(const std::string& text)>;
  void set_change_callback(ChangeCallback callback) { change_callback_ = callback; }

private:
  // ========== 文本处理 ==========

  void split_lines();  // 将 text_ 分割为 lines_
  void rebuild_text(); // 将 lines_ 合并回 text_

  int get_line_from_cursor() const;
  int get_column_from_cursor() const;
  void move_cursor_to_line_column(int line, int col);

  // ========== 渲染辅助 ==========

  void render_background(tvg::Scene* scene, const Element& elem);
  void render_text_lines(tvg::Scene* scene, const Element& elem);
  void render_placeholder(tvg::Scene* scene, const Element& elem);
  void render_selection(tvg::Scene* scene, const Element& elem);
  void render_cursor(tvg::Scene* scene, const Element& elem);

  // ========== 事件处理 ==========

  bool handle_mouse_down(const Event& event, Element& elem);
  bool handle_key_down(const Event& event, Element& elem);
  bool handle_text_input(const Event& event, Element& elem);

  void insert_text(const std::string& str);
  void delete_selection();
  void delete_char_before_cursor();
  void delete_char_after_cursor();

  // ========== 光标动画 ==========

  void update_cursor_blink(float delta_ms);

  // ========== 状态 ==========

  std::string text_;
  std::string placeholder_;
  std::vector<std::string> lines_;  // 文本按行分割

  int cursor_pos_ = 0;             // 光标位置（在整个文本中的索引）
  int selection_start_ = -1;       // 选择起点（-1 表示无选择）
  int selection_end_ = -1;

  bool readonly_ = false;
  bool disabled_ = false;

  // 光标闪烁动画
  float cursor_blink_time_ = 0.0f;
  bool cursor_visible_ = true;

  // 滚动偏移
  float scroll_offset_ = 0.0f;

  // 回调
  ChangeCallback change_callback_;
};

} // namespace tvgbox2

#endif // TVGBOX2_TEXTAREA_WIDGET_H

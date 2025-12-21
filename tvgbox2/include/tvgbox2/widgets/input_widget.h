/*
 * tvgbox2 - InputWidget
 *
 * 文本输入控件
 */

#ifndef TVGBOX2_INPUT_WIDGET_H
#define TVGBOX2_INPUT_WIDGET_H

#include "../textedit.h"
#include "../widget.h"
#include <functional>
#include <memory>
#include <string>

namespace tvgbox2 {

/**
 * InputWidget - 文本输入框
 *
 * 支持功能：
 * - 文本输入、删除、光标移动
 * - 鼠标选择文本
 * - Ctrl+A 全选、Ctrl+C/X/V 复制粘贴
 * - placeholder 文本
 * - password 模式（显示 *）
 * - 样式通过 CSS 变量：
 *   --input-bg: 背景色
 *   --input-text: 文字颜色
 *   --input-border: 边框颜色
 *   --input-placeholder: placeholder 颜色
 *   --input-selection-bg: 选中背景色
 *   --input-cursor: 光标颜色
 */
class InputWidget : public Widget {
public:
  /**
   * 构造函数
   *
   * @param placeholder 占位符文本
   * @param password 是否为密码模式
   */
  explicit InputWidget(const std::string &placeholder = "", bool password = false);

  // ========================================================================
  // Widget 接口实现
  // ========================================================================

  void render(tvg::Scene *scene, const Element &elem, Renderer &renderer) override;
  bool handle_event(const Event &event, Element &elem) override;
  void update(float delta_ms, Element &elem) override;
  const char *type_name() const override { return "InputWidget"; }

  // ========================================================================
  // 文本访问
  // ========================================================================

  const std::string &text() const { return text_; }
  void set_text(const std::string &text);

  const std::string &placeholder() const { return placeholder_; }
  void set_placeholder(const std::string &text) {
    placeholder_ = text;
    dirty_ = true;
  }

  bool is_password() const { return password_; }
  void set_password(bool enabled) {
    password_ = enabled;
    dirty_ = true;
  }

  // ========================================================================
  // 选择和光标
  // ========================================================================

  size_t cursor_pos() const { return textedit_ ? textedit_->cursor() : 0; }
  void set_cursor_pos(size_t pos);

  bool has_selection() const { return textedit_ && textedit_->has_selection(); }
  std::string selected_text() const;
  void select_all();
  void clear_selection();

  // ========================================================================
  // 回调
  // ========================================================================

  using ChangeCallback = std::function<void(const std::string &text)>;
  void set_change_callback(ChangeCallback callback) { change_callback_ = callback; }

private:
  // ========== 渲染辅助 ==========

  void render_background(tvg::Scene *scene, const Element &elem);
  void render_text(tvg::Scene *scene, const Element &elem);
  void render_cursor(tvg::Scene *scene, const Element &elem);
  void render_selection(tvg::Scene *scene, const Element &elem);

  // ========== 事件处理 ==========

  bool handle_key_down(const Event &event, Element &elem);
  bool handle_text_input(const Event &event, Element &elem);
  bool handle_mouse_down(const Event &event, Element &elem);
  bool handle_mouse_move(const Event &event, Element &elem);
  bool handle_mouse_up(const Event &event, Element &elem);

  // ========== 辅助 ==========

  // 坐标转换：屏幕 x → 文本索引
  size_t x_to_index(float x, const Element &elem) const;

  // 文本索引 → 屏幕 x
  float index_to_x(size_t index, const Element &elem) const;

  // 获取显示文本（如果是密码模式，返回 ***）
  std::string display_text() const;

  // 初始化 TextEdit（延迟初始化）
  void ensure_textedit_init(const Element &elem);

  // ========== 状态 ==========

  std::string text_;
  std::string placeholder_;
  bool password_ = false;

  // stb_textedit 引擎
  std::unique_ptr<TextEdit> textedit_;
  bool textedit_initialized_ = false;
  float char_width_ = 0; // 缓存的字符宽度

  ChangeCallback change_callback_;

  bool is_dragging_ = false; // 是否正在拖拽选择

  // 光标闪烁动画
  float cursor_blink_time_ = 0;
  bool cursor_visible_ = true;
};

} // namespace tvgbox2

#endif // TVGBOX2_INPUT_WIDGET_H

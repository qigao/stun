/*
 * flexUI - Widget Base Class
 *
 * 所有交互式 UI 组件的基类
 */

#ifndef FLEXUI_WIDGET_H
#define FLEXUI_WIDGET_H

#include <flexUI/types.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace flexUI {

// 前向声明
class Element;
struct Event;
class RenderCommandList;

/**
 * Widget - 所有 UI 组件的基类
 *
 * 设计原则：
 * 1. Widget 拥有自己的状态（不污染 Element）
 * 2. Widget 知道如何渲染自己
 * 3. Widget 知道如何响应事件
 * 4. Widget 通过 ComputedStyle 读取样式
 */
class Widget {
public:
  virtual ~Widget() = default;

  void bind_host_element(Element* elem);

  /**
   * 发射 Widget 绘制命令。
   *
   * Widget 只描述绘制意图，不直接触达后端 renderer。
   */
  virtual void emit_render_commands(const Element& elem,
                                    RenderCommandList& commands) = 0;

  /**
   * 发射 Overlay 绘制命令（在所有元素之后渲染，用于 dropdown、popup 等）。
   */
  virtual void emit_overlay_commands(const Element& elem,
                                     RenderCommandList& commands) {
    (void)elem;
    (void)commands;
  }

  /**
   * 检查是否有 overlay 需要渲染
   */
  virtual bool has_overlay() const { return false; }

  /**
   * 处理事件
   *
   * @return true 如果事件被消费（停止传播）
   */
  virtual bool handle_event(const Event& event, Element& elem) {
    return false;  // 默认不消费
  }

  /**
   * 更新动画/定时器（每帧调用）
   *
   * @param delta_ms 距离上一帧的时间（毫秒）
   */
  virtual void update(float delta_ms, Element& elem) {
    // 默认不做任何事
  }

  /**
   * 是否需要参与逐帧 update。
   *
   * 默认保持旧行为；确定没有动画、计时器或拖拽状态的 widget 可以返回 false。
   */
  virtual bool needs_frame_update(const Element& elem) const {
    (void)elem;
    return true;
  }

  /**
   * 宿主 pseudo-state 是否会改变 widget 自身绘制。
   *
   * CSS selector 依赖由 StyleEngine 处理；这个钩子只描述 widget 内部
   * 是否直接读取该 state 来改变绘制结果。
   */
  virtual bool state_affects_paint(Symbol state) const {
    (void)state;
    return true;
  }

  /**
   * 在 layout 之后同步宿主语义。
   *
   * 某些 widget 的 host attribute / pseudo-state 依赖已决议的尺寸、
   * 内容范围或滚动能力，不能只在 bind/render 时同步。
   */
  virtual void sync_host_semantics_for_layout(Element& elem) {
    (void)elem;
    sync_host_semantics();
  }

  /**
   * 获取 Widget 类型名（用于调试）
   */
  virtual const char* type_name() const = 0;

  /**
   * Does this widget want to capture all mouse events?
   *
   * When true, mouse events are routed to this widget first,
   * regardless of hit-testing. Used for overlay widgets like
   * dropdowns, modals, and popups.
   *
   * @return true if the widget should capture mouse events
   */
  virtual bool wants_mouse_capture() const { return false; }

  /**
   * Does this widget want to receive text input / IME?
   */
  virtual bool wants_text_input() const { return false; }

  /**
   * Whether the widget renders the host box chrome itself.
   *
   * Widgets that consume CSS box tokens internally can opt out of the generic
   * element background/border pass to avoid duplicate host-box paints.
   */
  virtual bool paints_host_box() const { return false; }

  /**
   * Complex stable parts may keep their identity in the Element tree while
   * delegating atomic leaf drawing (glyphs, carets, selection fragments) to
   * the owning widget.
   */
  virtual bool paints_part_box(std::string_view part_name) const {
    (void)part_name;
    return false;
  }
  virtual bool emit_part_render_commands(const Element& host,
                                         const Element& part,
                                         std::string_view part_name,
                                         RenderCommandList& commands) {
    (void)host;
    (void)part;
    (void)part_name;
    (void)commands;
    return false;
  }

  void set_semantic_tree_rendering(bool active) {
    semantic_tree_rendering_ = active;
  }

  /**
   * Get the current caret rectangle in local coordinates.
   * Used for positioning the IME candidate window.
   */
  virtual void get_caret_rect(const Element& elem, float& x, float& y, float& w, float& h) const {}

  /**
   * 返回 widget 叶子的固有内容尺寸（不含 host padding/border/margin）。
   *
   * layout 主链会在 auto width / auto height 时消费它，避免把渲染 bounds
   * 当作布局事实源。
   *
   * @return true 表示成功提供尺寸
   */
  virtual bool measure_intrinsic_size(const Element& elem, float available_width,
                                      float available_height, float& out_width,
                                      float& out_height) const {
    (void)elem;
    (void)available_width;
    (void)available_height;
    (void)out_width;
    (void)out_height;
    return false;
  }

protected:
  Element* host_element();
  const Element* host_element() const;
  Element* create_part(const char* tag, const char* name);
  Element* part(const char* name);
  const Element* part(const char* name) const;
  void set_host_attribute(const char* name, const std::string& value);
  void clear_host_attribute(const char* name);
  void set_host_presence_attribute(const char* name, bool enabled);
  void set_host_boolean_attribute(const char* name, bool enabled);
  void set_host_state(const char* state, bool active);
  void set_host_data_state(const char* active_value, const char* inactive_value,
                           bool active);
  virtual void sync_host_semantics() {}
  virtual void build_semantic_tree() {}
  bool is_semantic_tree_rendering() const { return semantic_tree_rendering_; }

  // Widget 内部脏标记（避免每帧重新渲染）
  bool dirty_ = true;

private:
  Element* host_element_ = nullptr;
  bool semantic_tree_rendering_ = false;
  std::unordered_map<Symbol, Element*, SymbolHash> parts_;
};

} // namespace flexUI

#endif // FLEXUI_WIDGET_H

/*
 * flexUI - Widget Base Class
 *
 * 所有交互式 UI 组件的基类
 */

#ifndef FLEXUI_WIDGET_H
#define FLEXUI_WIDGET_H

namespace flexUI {

// 前向声明
struct Element;
struct Event;
class Renderer;

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

  /**
   * 渲染 Widget
   *
   * @param elem 关联的 Element（用于获取样式、布局）
   * @param renderer 渲染器（后端无关）
   */
  virtual void render(const Element& elem, Renderer& renderer) = 0;

  /**
   * 渲染 Overlay（在所有元素之后渲染，用于 dropdown、popup 等）
   * 默认不渲染任何 overlay
   */
  virtual void render_overlay(const Element& elem, Renderer& renderer) {}

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

protected:
  // Widget 内部脏标记（避免每帧重新渲染）
  bool dirty_ = true;
};

} // namespace flexUI

#endif // FLEXUI_WIDGET_H

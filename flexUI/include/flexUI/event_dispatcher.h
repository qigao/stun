/*
 * flexUI - EventDispatcher
 *
 * 事件分发系统：hit testing、事件路由、状态管理
 */

#ifndef FLEXUI_EVENT_DISPATCHER_H
#define FLEXUI_EVENT_DISPATCHER_H

#include "event.h"
#include <functional>
#include <utility>
#include <vector>

namespace flexUI {

class Element;
class Box;

/**
 * EventDispatcher - 事件分发器
 *
 * 职责：
 * 1. Hit testing（坐标 → 元素）
 * 2. 事件路由和冒泡
 * 3. hover/active/focus 状态管理
 * 4. 鼠标捕获
 */
class EventDispatcher {
public:
  using EventCallback = std::function<void(Element &, const Event &)>;

  explicit EventDispatcher(Box *box) : box_(box) {}

  // 分发事件（主入口）
  void dispatch(Event &event, Element *root);

  // 焦点管理
  void set_focus(Element *elem);
  Element *focused_element() const { return focused_; }

  // 鼠标捕获
  void set_capture(Element *elem) { capturing_ = elem; }
  void release_capture(Element *elem) {
    if (capturing_ == elem)
      capturing_ = nullptr;
  }
  Element *capturing_element() const { return capturing_; }

  // 状态查询
  Element *hovered_element() const { return hovered_; }
  Element *active_element() const { return active_; }
  void detach_subtree(Element *root);

  // 全局事件回调
  void set_event_callback(EventCallback cb) { callback_ = cb; }

  /// Installs the framework-owned observer used by higher-level application
  /// runtimes. It is independent from the public application callback and is
  /// invoked only after the current widget declines to consume the event.
  void set_framework_event_callback(EventCallback cb) { framework_callback_ = std::move(cb); }

private:
  // Hit testing
  Element *hit_test(Element *elem, float x, float y);

  // 事件处理
  Element *handle_special_events(Event &event, Element *pointer_hit_target);
  void propagate_event(Event &event, Element *target);
  void notify_framework_event(const Event &event, Element *target);

  // 辅助
  std::vector<Element *> get_ancestors(Element *elem);

  Box *box_;

  // 状态
  Element *hovered_ = nullptr;
  Element *active_ = nullptr;
  Element *focused_ = nullptr;
  Element *capturing_ = nullptr;
  bool prefers_focus_visible_ = false;

  EventCallback callback_;
  EventCallback framework_callback_;
};

} // namespace flexUI

#endif // FLEXUI_EVENT_DISPATCHER_H

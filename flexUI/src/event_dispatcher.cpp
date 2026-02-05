/*
 * flexUI - EventDispatcher Implementation
 */

#include <flexUI/event_dispatcher.h>
#include <flexUI/element.h>
#include <flexUI/widget.h>
#include <flexUI/box.h>

namespace flexUI {

void EventDispatcher::dispatch(Event& event, Element* root) {
  if (!root) return;

  Element* target = nullptr;

  // 鼠标事件路由
  if (event.type == EventType::MouseMove ||
      event.type == EventType::MouseDown ||
      event.type == EventType::MouseUp ||
      event.type == EventType::MouseWheel) {

    // 优先检查捕获元素
    if (capturing_) {
      event.target = capturing_;
      if (capturing_->widget) {
        bool consumed = capturing_->widget->handle_event(event, *capturing_);
        if (consumed) {
          event.handled = true;
          return;
        }
      }
    }

    target = hit_test(root, event.x, event.y);
  }
  // 键盘事件路由到焦点元素
  else if (event.type == EventType::KeyDown ||
           event.type == EventType::KeyUp ||
           event.type == EventType::TextInput) {
    target = focused_;
  }

  event.target = target;

  // 处理状态变化
  handle_special_events(event, target);

  // 事件冒泡
  if (target) {
    propagate_event(event, target);
  }

  // 更新捕获状态
  if (target && target->widget) {
    if (target->widget->wants_mouse_capture()) {
      capturing_ = target;
    } else if (capturing_ == target) {
      capturing_ = nullptr;
    }
  }
  if (capturing_ && capturing_->widget &&
      !capturing_->widget->wants_mouse_capture()) {
    capturing_ = nullptr;
  }
}

void EventDispatcher::handle_special_events(Event& event, Element* target) {
  if (event.type == EventType::MouseMove) {
    // Hover 状态切换
    if (target != hovered_) {
      if (hovered_) {
        hovered_->set_hover(false);
        Event e = Event::mouse_move(event.x, event.y);
        e.type = EventType::MouseLeave;
        e.target = hovered_;
        if (hovered_->widget) {
          hovered_->widget->handle_event(e, *hovered_);
        }
      }

      hovered_ = target;
      if (hovered_) {
        hovered_->set_hover(true);
        Event e = Event::mouse_move(event.x, event.y);
        e.type = EventType::MouseEnter;
        e.target = hovered_;
        if (hovered_->widget) {
          hovered_->widget->handle_event(e, *hovered_);
        }
      }
    }
  }
  else if (event.type == EventType::MouseDown) {
    if (target) {
      active_ = target;
      target->set_active(true);
    }
    if (target && target->focusable) {
      set_focus(target);
    }
  }
  else if (event.type == EventType::MouseUp) {
    if (active_) {
      if (active_ == target && active_->click_callback()) {
        active_->click_callback()();
      }
      active_->set_active(false);
      active_ = nullptr;
    }
  }
}

void EventDispatcher::propagate_event(Event& event, Element* target) {
  auto ancestors = get_ancestors(target);

  for (Element* elem : ancestors) {
    if (event.handled && !event.propagate) break;

    if (elem->widget) {
      bool consumed = elem->widget->handle_event(event, *elem);
      if (consumed) {
        event.handled = true;
        event.propagate = false;
        break;
      }
    }

    if (callback_) {
      callback_(*elem, event);
    }
  }
}

void EventDispatcher::set_focus(Element* elem) {
  if (focused_ == elem) return;

  if (focused_) {
    focused_->set_focus(false);
    Event e = Event::focus_out();
    e.target = focused_;
    if (focused_->widget) {
      focused_->widget->handle_event(e, *focused_);
    }
  }

  focused_ = elem;
  if (focused_) {
    focused_->set_focus(true);
    Event e = Event::focus_in();
    e.target = focused_;
    if (focused_->widget) {
      focused_->widget->handle_event(e, *focused_);
    }
  }
}

Element* EventDispatcher::hit_test(Element* elem, float x, float y) {
  if (!elem || !elem->is_visible()) return nullptr;

  // 子元素优先（后渲染的在上层）
  const auto& ch = elem->children();
  for (auto it = ch.rbegin(); it != ch.rend(); ++it) {
    if (auto* child = static_cast<Element*>(*it)) {
      Element* hit = hit_test(child, x, y);
      if (hit) return hit;
    }
  }

  // 检查当前元素边界
  float ex = elem->absolute_x();
  float ey = elem->absolute_y();

  if (x >= ex && x <= ex + elem->width() &&
      y >= ey && y <= ey + elem->height()) {
    return elem;
  }

  return nullptr;
}

std::vector<Element*> EventDispatcher::get_ancestors(Element* elem) {
  std::vector<Element*> ancestors;
  while (elem) {
    ancestors.push_back(elem);
    elem = elem->parent_elem();
  }
  return ancestors;
}

} // namespace flexUI

/*
 * flexUI - EventDispatcher Implementation
 */

#include <flexUI/event_dispatcher.h>
#include <flexUI/element.h>
#include <flexUI/widget.h>
#include <flexUI/box.h>
#include <flexUI/detail/css_length.h>
#include <flexUI/detail/css_render_transform.h>
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <vector>

namespace flexUI {

namespace {

using flex::operator*;

constexpr float kWheelScrollStep = 40.0f;
constexpr float kScrollEpsilon = 0.001f;

struct ScrollInsets {
  float top = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;
  float left = 0.0f;
};

struct ClipRect {
  bool enabled = false;
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
};

bool can_scroll_axis(float offset, float max_scroll, float delta) {
  if (max_scroll <= 0.0f || std::fabs(delta) <= kScrollEpsilon) {
    return false;
  }
  if (delta > 0.0f) {
    return offset + kScrollEpsilon < max_scroll;
  }
  return offset > kScrollEpsilon;
}

bool can_scroll_x(const Element* elem, float delta) {
  return elem && elem->can_scroll_x() &&
         can_scroll_axis(elem->scroll_x(), elem->max_scroll_x(), delta);
}

bool can_scroll_y(const Element* elem, float delta) {
  return elem && elem->can_scroll_y() &&
         can_scroll_axis(elem->scroll_y(), elem->max_scroll_y(), delta);
}

bool scroll_from_wheel(Element* elem, const Event& event) {
  if (!elem) {
    return false;
  }

  const float dx = -event.delta_x * kWheelScrollStep;
  const float dy = -event.delta_y * kWheelScrollStep;

  if (can_scroll_x(elem, dx) && elem->scroll_by(dx, 0.0f)) {
    return true;
  }
  if (can_scroll_y(elem, dy) && elem->scroll_by(0.0f, dy)) {
    return true;
  }
  if (!can_scroll_y(elem, dy) && can_scroll_x(elem, dy) && elem->scroll_by(dy, 0.0f)) {
    return true;
  }

  return false;
}

bool scroll_from_key(Element* elem, const Event& event) {
  if (!elem || event.type != EventType::KeyDown) {
    return false;
  }

  switch (event.key) {
    case KeyCode::PageDown: {
      if (elem->can_scroll_y()) {
        return elem->scroll_by(0.0f, std::max(elem->height(), 0.0f));
      }
      if (elem->can_scroll_x()) {
        return elem->scroll_by(std::max(elem->width(), 0.0f), 0.0f);
      }
      return false;
    }
    case KeyCode::PageUp: {
      if (elem->can_scroll_y()) {
        return elem->scroll_by(0.0f, -std::max(elem->height(), 0.0f));
      }
      if (elem->can_scroll_x()) {
        return elem->scroll_by(-std::max(elem->width(), 0.0f), 0.0f);
      }
      return false;
    }
    case KeyCode::Home: {
      if (elem->can_scroll_y()) {
        return elem->set_scroll_offset(elem->scroll_x(), 0.0f);
      }
      if (elem->can_scroll_x()) {
        return elem->set_scroll_offset(0.0f, elem->scroll_y());
      }
      return false;
    }
    case KeyCode::End: {
      if (elem->can_scroll_y()) {
        return elem->set_scroll_offset(elem->scroll_x(), elem->max_scroll_y());
      }
      if (elem->can_scroll_x()) {
        return elem->set_scroll_offset(elem->max_scroll_x(), elem->scroll_y());
      }
      return false;
    }
    default:
      return false;
  }
}

float resolve_clip_length(const Box* box, const Element* elem,
                          const std::string& value,
                          float percent_reference) {
  detail::CssLengthContext context;
  context.percent_reference = percent_reference;
  if (box) {
    context.viewport_width = box->viewport_width();
    context.viewport_height = box->viewport_height();
  } else if (elem && elem->owner_box_) {
    context.viewport_width = elem->owner_box_->viewport_width();
    context.viewport_height = elem->owner_box_->viewport_height();
  }
  const float resolved = detail::parse_css_length(value, context);
  return std::isnan(resolved) ? detail::css_nan() : resolved;
}

ClipRect resolve_clip_path_inset(const Box* box, const Element* elem) {
  ClipRect clip;
  if (!elem || !elem->computed_style || elem->width() <= 0.0f ||
      elem->height() <= 0.0f) {
    return clip;
  }

  const std::string value = detail::lower_css_copy(detail::trim_css_copy(
      elem->computed_style->get_variable(Symbol("--clip-path"), "")));
  if (value.empty() || value == "none") {
    return clip;
  }

  static const std::string prefix = "inset(";
  if (value.rfind(prefix, 0) != 0 || value.back() != ')') {
    return clip;
  }

  std::string inner = detail::trim_css_copy(
      value.substr(prefix.size(), value.size() - prefix.size() - 1));
  auto tokens = detail::split_css_tokens(inner);
  const auto round_it = std::find(tokens.begin(), tokens.end(), "round");
  if (round_it != tokens.end()) {
    tokens.erase(round_it, tokens.end());
  }
  if (tokens.empty() || tokens.size() > 4) {
    return clip;
  }

  const auto token_at = [&](size_t index) -> const std::string& {
    switch (tokens.size()) {
      case 1:
        return tokens[0];
      case 2:
        return tokens[index % 2];
      case 3:
        return index == 3 ? tokens[1] : tokens[index];
      default:
        return tokens[index];
    }
  };

  const float top = resolve_clip_length(box, elem, token_at(0), elem->height());
  const float right = resolve_clip_length(box, elem, token_at(1), elem->width());
  const float bottom = resolve_clip_length(box, elem, token_at(2), elem->height());
  const float left = resolve_clip_length(box, elem, token_at(3), elem->width());
  if (std::isnan(top) || std::isnan(right) || std::isnan(bottom) ||
      std::isnan(left)) {
    return clip;
  }

  clip.enabled = true;
  clip.x = left;
  clip.y = top;
  clip.width = std::max(0.0f, elem->width() - left - right);
  clip.height = std::max(0.0f, elem->height() - top - bottom);
  return clip;
}

float resolve_scroll_length(const Box* box, const Element* elem,
                            const std::string& value,
                            float percent_reference) {
  detail::CssLengthContext context;
  context.percent_reference = percent_reference;
  if (box) {
    context.viewport_width = box->viewport_width();
    context.viewport_height = box->viewport_height();
  } else if (elem && elem->owner_box_) {
    context.viewport_width = elem->owner_box_->viewport_width();
    context.viewport_height = elem->owner_box_->viewport_height();
  }
  const float resolved = detail::parse_css_length(value, context);
  return std::isnan(resolved) ? 0.0f : std::max(resolved, 0.0f);
}

ScrollInsets resolve_scroll_insets(const Box* box, const Element* elem,
                                   const char* prefix) {
  ScrollInsets insets;
  if (!elem || !elem->computed_style) {
    return insets;
  }

  const std::string root(prefix);
  const auto resolve = [&](const char* side, float percent_reference) {
    return resolve_scroll_length(
        box, elem,
        elem->computed_style->get_variable(Symbol(root + side), ""),
        percent_reference);
  };

  insets.top = resolve("-top", elem->height());
  insets.right = resolve("-right", elem->width());
  insets.bottom = resolve("-bottom", elem->height());
  insets.left = resolve("-left", elem->width());
  return insets;
}

void reveal_focus_in_scroll_ancestors(Box* box, Element* focused) {
  if (!focused) {
    return;
  }

  for (Element* ancestor = focused->parent_elem(); ancestor;
       ancestor = ancestor->parent_elem()) {
    if (!ancestor->is_scroll_container()) {
      continue;
    }
    const ScrollInsets padding =
        resolve_scroll_insets(box, ancestor, "--scroll-padding");
    const ScrollInsets margin =
        resolve_scroll_insets(box, focused, "--scroll-margin");
    const float scroll_padding[4] = {padding.top, padding.right, padding.bottom,
                                     padding.left};
    const float scroll_margin[4] = {margin.top, margin.right, margin.bottom,
                                    margin.left};
    ancestor->scroll_descendant_into_view(focused, scroll_padding, scroll_margin);
  }
}

template <typename ScrollFn>
bool dispatch_scroll_to_ancestor(Element* start, ScrollFn&& scroll_fn,
                                 Event& event) {
  for (Element* elem = start; elem; elem = elem->parent_elem()) {
    if (!elem->can_scroll_x() && !elem->can_scroll_y()) {
      continue;
    }
    if (!scroll_fn(elem)) {
      continue;
    }

    event.target = elem;
    event.handled = true;
    event.propagate = false;
    return true;
  }
  return false;
}

Element* hit_test_element(Box* box, Element* elem, float x, float y,
                          const flex::Transform& parent_transform) {
  if (!elem || !elem->is_visible()) return nullptr;

  const flex::Transform element_transform =
      parent_transform * detail::local_css_render_transform(elem);
  const flex::Vec2 local_pos =
      flex::inverse(element_transform) * flex::Vec2(x, y);
  const bool inside = local_pos.x >= 0.0f && local_pos.x <= elem->width() &&
                      local_pos.y >= 0.0f && local_pos.y <= elem->height();
  const bool clip_children = elem->computed_style &&
      (elem->computed_style->overflow_x == Overflow::Hidden ||
       elem->computed_style->overflow_y == Overflow::Hidden ||
       elem->computed_style->overflow_x == Overflow::Auto ||
       elem->computed_style->overflow_y == Overflow::Auto ||
       elem->computed_style->overflow_x == Overflow::Scroll ||
       elem->computed_style->overflow_y == Overflow::Scroll);
  if (clip_children && !inside) {
    return nullptr;
  }

  const ClipRect clip_path = resolve_clip_path_inset(box, elem);
  if (clip_path.enabled) {
    const bool inside_clip =
        local_pos.x >= clip_path.x &&
        local_pos.x <= clip_path.x + clip_path.width &&
        local_pos.y >= clip_path.y &&
        local_pos.y <= clip_path.y + clip_path.height;
    if (!inside_clip) {
      return nullptr;
    }
  }

  flex::Transform content_transform = element_transform;
  if (elem->scroll_x() != 0.0f || elem->scroll_y() != 0.0f) {
    content_transform = content_transform *
                        flex::make_translation(-elem->scroll_x(),
                                               -elem->scroll_y());
  }

  const auto& ch = elem->children();
  bool needs_sort = false;
  int previous_z = 0;
  bool have_previous_z = false;
  for (auto* node : ch) {
    auto* child = static_cast<Element*>(node);
    if (!child) {
      continue;
    }
    const int child_z = child->z_index();
    if (have_previous_z && child_z < previous_z) {
      needs_sort = true;
      break;
    }
    previous_z = child_z;
    have_previous_z = true;
  }

  if (needs_sort) {
    std::vector<Element*> sorted_children;
    sorted_children.reserve(ch.size());
    for (auto* node : ch) {
      if (auto* child = static_cast<Element*>(node)) {
        sorted_children.push_back(child);
      }
    }
    std::stable_sort(sorted_children.begin(), sorted_children.end(),
                     [](const Element* lhs, const Element* rhs) {
                       return lhs->z_index() < rhs->z_index();
                     });
    for (auto it = sorted_children.rbegin(); it != sorted_children.rend(); ++it) {
      Element* hit = hit_test_element(box, *it, x, y, content_transform);
      if (hit) return hit;
    }
  } else {
    for (auto it = ch.rbegin(); it != ch.rend(); ++it) {
      if (auto* child = static_cast<Element*>(*it)) {
        Element* hit = hit_test_element(box, child, x, y, content_transform);
        if (hit) return hit;
      }
    }
  }

  const bool accepts_pointer_events =
      !elem->computed_style ||
      elem->computed_style->get_variable(Symbol("pointer-events"), "") != "none";

  if (inside && accepts_pointer_events) {
    return elem;
  }

  return nullptr;
}

} // namespace

void EventDispatcher::dispatch(Event& event, Element* root) {
  if (!root) return;

  if (event.type == EventType::KeyDown || event.type == EventType::KeyUp ||
      event.type == EventType::TextInput ||
      event.type == EventType::CompositionStart ||
      event.type == EventType::CompositionUpdate ||
      event.type == EventType::CompositionEnd) {
    prefers_focus_visible_ = true;
  } else if (event.type == EventType::MouseDown) {
    prefers_focus_visible_ = false;
  }

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
           event.type == EventType::TextInput ||
           event.type == EventType::CompositionStart ||
           event.type == EventType::CompositionUpdate ||
           event.type == EventType::CompositionEnd) {
    target = focused_;
  }

  event.target = target;

  // 处理状态变化
  handle_special_events(event, target);

  // 事件冒泡
  if (target) {
    propagate_event(event, target);
  }

  if (event.type == EventType::MouseWheel && !event.handled) {
    dispatch_scroll_to_ancestor(target,
                                [&](Element* elem) {
                                  return scroll_from_wheel(elem, event);
                                },
                                event);
  } else if (event.type == EventType::KeyDown && !event.handled) {
    dispatch_scroll_to_ancestor(target,
                                [&](Element* elem) {
                                  return scroll_from_key(elem, event);
                                },
                                event);
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

  std::unordered_set<Element*> next_focus_chain;
  for (Element* current = elem; current; current = current->parent_elem()) {
    next_focus_chain.insert(current);
  }

  for (Element* current = focused_; current; current = current->parent_elem()) {
    if (next_focus_chain.find(current) == next_focus_chain.end()) {
      current->set_state("focus-within", false);
    }
  }

  if (focused_) {
    focused_->set_focus(false);
    focused_->set_focus_visible(false);
    Event e = Event::focus_out();
    e.target = focused_;
    if (focused_->widget) {
      focused_->widget->handle_event(e, *focused_);
    }
  }

  focused_ = elem;
  if (focused_) {
    focused_->set_focus(true);
    focused_->set_focus_visible(prefers_focus_visible_);
    for (Element* current = focused_; current; current = current->parent_elem()) {
      current->set_state("focus-within", true);
    }
    reveal_focus_in_scroll_ancestors(box_, focused_);
    Event e = Event::focus_in();
    e.target = focused_;
    if (focused_->widget) {
      focused_->widget->handle_event(e, *focused_);
    }
  }
}

Element* EventDispatcher::hit_test(Element* elem, float x, float y) {
  Element* hit = hit_test_element(box_, elem, x, y, flex::Transform{});
  while (hit && hit->is_widget_owned()) {
    hit = hit->parent_elem();
  }
  return hit;
}

void EventDispatcher::detach_subtree(Element* root) {
  if (!root) {
    return;
  }
  const auto is_in_subtree = [root](Element* element) {
    for (Element* current = element; current; current = current->parent_elem()) {
      if (current == root) {
        return true;
      }
    }
    return false;
  };

  if (is_in_subtree(focused_)) {
    set_focus(nullptr);
  }
  if (is_in_subtree(hovered_)) {
    hovered_->set_hover(false);
    hovered_ = nullptr;
  }
  if (is_in_subtree(active_)) {
    active_->set_active(false);
    active_ = nullptr;
  }
  if (is_in_subtree(capturing_)) {
    capturing_ = nullptr;
  }
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

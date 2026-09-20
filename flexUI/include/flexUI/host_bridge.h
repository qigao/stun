#pragma once

#include <flexUI/box.h>
#include <flexUI/detail/css_render_transform.h>
#include <flexUI/event.h>

#include <string>
#include <utility>

namespace flexUI::host {

using flex::operator*;

inline Element* interaction_target_element(Box* box) {
  if (!box) return nullptr;
  if (auto* capturing = box->capturing_element()) return capturing;
  if (auto* hovered = box->hovered_element()) return hovered;
  return box->focused_element();
}

inline std::string requested_cursor(Box* box,
                                    const std::string& fallback = "default") {
  auto* target = interaction_target_element(box);
  if (!target || !target->computed_style) return fallback;
  const std::string cursor =
      target->computed_style->get_variable(Symbol("--cursor"), "");
  return cursor.empty() ? fallback : cursor;
}

inline std::string requested_appearance(Box* box,
                                        const std::string& fallback = "auto") {
  auto* target = interaction_target_element(box);
  if (!target || !target->computed_style) return fallback;
  const std::string appearance =
      target->computed_style->get_variable(Symbol("--appearance"), "");
  return appearance.empty() ? fallback : appearance;
}

inline bool should_use_native_form_appearance(Box* box) {
  const std::string appearance = requested_appearance(box);
  return appearance != "none";
}

template <typename SetCursorFn>
inline bool sync_cursor(Box* box,
                        const std::string& host_cursor,
                        SetCursorFn&& set_cursor) {
  const std::string desired = requested_cursor(box);
  if (desired == host_cursor) return false;
  std::forward<SetCursorFn>(set_cursor)(desired);
  return true;
}

inline std::string requested_user_select(Box* box,
                                         const std::string& fallback = "auto") {
  auto* target = interaction_target_element(box);
  if (!target || !target->computed_style) return fallback;
  const std::string policy =
      target->computed_style->get_variable(Symbol("--user-select"), "");
  return policy.empty() ? fallback : policy;
}

inline bool should_enable_native_text_selection(Box* box) {
  return requested_user_select(box) != "none";
}

inline std::string requested_touch_action(Box* box,
                                          const std::string& fallback = "auto") {
  auto* target = interaction_target_element(box);
  if (!target || !target->computed_style) return fallback;
  const std::string action =
      target->computed_style->get_variable(Symbol("--touch-action"), "");
  return action.empty() ? fallback : action;
}

inline bool should_disable_native_touch_actions(Box* box) {
  return requested_touch_action(box) == "none";
}

inline std::string requested_color_scheme(Box* box,
                                          const std::string& fallback = "normal") {
  auto* target = interaction_target_element(box);
  if (!target || !target->computed_style) return fallback;
  const std::string scheme =
      target->computed_style->get_variable(Symbol("--color-scheme"), "");
  return scheme.empty() ? fallback : scheme;
}

inline bool should_prefer_dark_color_scheme(Box* box) {
  const std::string scheme = requested_color_scheme(box);
  return scheme.find("dark") != std::string::npos &&
         scheme.find("light") == std::string::npos;
}

inline std::string requested_document_color_scheme(
    Box* box, const std::string& fallback = "normal") {
  if (!box) return fallback;
  auto* root = box->root();
  if (!root || !root->computed_style) return fallback;
  const std::string scheme =
      root->computed_style->get_variable(Symbol("--color-scheme"), "");
  return scheme.empty() ? fallback : scheme;
}

inline bool should_prefer_dark_document_color_scheme(Box* box) {
  if (!box) return false;
  const std::string scheme = requested_document_color_scheme(box);
  const bool has_dark = scheme.find("dark") != std::string::npos;
  const bool has_light = scheme.find("light") != std::string::npos;
  if (has_dark && !has_light) return true;
  if (has_light && !has_dark) return false;
  if (has_dark && has_light) return box->media_environment().prefers_dark_scheme;
  return box->media_environment().prefers_dark_scheme;
}

inline bool is_visible_in_host_tree(const Element* elem) {
  for (const Element* current = elem; current;
       current = current->parent_elem()) {
    if (!current->is_visible()) return false;
  }
  return true;
}

inline Element* focused_text_input_element(Box* box) {
  if (!box) return nullptr;
  auto* focused = box->focused_element();
  if (!focused || !is_visible_in_host_tree(focused) || !focused->widget ||
      !focused->widget->wants_text_input()) {
    return nullptr;
  }
  return focused;
}

inline bool box_wants_text_input(Box* box) {
  return focused_text_input_element(box) != nullptr;
}

inline bool dispatch_text_input_if_focused(Box* box, const std::string& utf8) {
  if (!box_wants_text_input(box) || utf8.empty() || !validate_utf8(utf8).valid) return false;
  auto event = Event::text_input(utf8);
  box->dispatch_event(event);
  return true;
}

inline bool dispatch_composition_start_if_focused(Box* box) {
  if (!box_wants_text_input(box)) return false;
  auto event = Event::composition_start();
  box->dispatch_event(event);
  return true;
}

inline bool dispatch_composition_update_if_focused(Box* box, const std::string& text) {
  if (!box_wants_text_input(box)) return false;
  auto event = Event::composition_update(text);
  box->dispatch_event(event);
  return true;
}

inline bool dispatch_composition_end_if_focused(Box* box) {
  if (!box_wants_text_input(box)) return false;
  auto event = Event::composition_end();
  box->dispatch_event(event);
  return true;
}

inline bool focused_text_input_caret_anchor(Box* box, float& x, float& y) {
  auto* focused = focused_text_input_element(box);
  if (!focused) return false;

  float local_x = 0.0f;
  float local_y = 0.0f;
  float local_w = 0.0f;
  float local_h = 0.0f;
  focused->widget->get_caret_rect(*focused, local_x, local_y, local_w, local_h);

  const auto screen_pos =
      detail::css_render_world_transform(focused) *
      flex::Vec2(local_x, local_y + local_h);
  x = screen_pos.x;
  y = screen_pos.y;
  return true;
}

inline bool clear_mouse_capture(Box* box) {
  if (!box) return false;
  auto* capturing = box->capturing_element();
  if (!capturing) return false;
  box->release_mouse_capture(capturing);
  return true;
}

inline bool clear_focus(Box* box) {
  if (!box || !box->focused_element()) return false;
  box->set_focus(nullptr);
  return true;
}

inline bool clear_focus_and_capture(Box* box) {
  const bool capture_cleared = clear_mouse_capture(box);
  const bool focus_cleared = clear_focus(box);
  return capture_cleared || focus_cleared;
}

template <typename AcquireFn, typename ReleaseFn>
inline bool sync_mouse_capture(Box* box,
                               bool host_has_capture,
                               AcquireFn&& acquire_capture,
                               ReleaseFn&& release_capture) {
  const bool ui_wants_capture = box && box->capturing_element();
  if (ui_wants_capture) {
    if (!host_has_capture) {
      std::forward<AcquireFn>(acquire_capture)();
      return true;
    }
    return false;
  }

  if (host_has_capture) {
    std::forward<ReleaseFn>(release_capture)();
    return true;
  }
  return false;
}

}  // namespace flexUI::host

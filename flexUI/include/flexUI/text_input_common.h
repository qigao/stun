#ifndef FLEXUI_TEXT_INPUT_COMMON_H
#define FLEXUI_TEXT_INPUT_COMMON_H

#include <flexUI/clipboard.h>
#include <flexUI/element.h>
#include <flexUI/event.h>

#include <string>
#include <type_traits>
#include <utility>

namespace flexUI::text_input_common {

inline void reset_cursor_blink(float& cursor_blink_time, bool& cursor_visible) {
  cursor_blink_time = 0.0f;
  cursor_visible = true;
}

inline void finish_cursor_change(
    float& cursor_blink_time,
    bool& cursor_visible,
    bool* dirty = nullptr,
    Element* elem = nullptr) {
  reset_cursor_blink(cursor_blink_time, cursor_visible);
  if (dirty) {
    *dirty = true;
  }
  if (elem) {
    elem->mark_paint_dirty();
  }
}

inline bool update_cursor_blink(
    bool focused,
    float delta_ms,
    float period_ms,
    float& cursor_blink_time,
    bool& cursor_visible) {
  if (!focused) {
    const bool changed = cursor_visible || cursor_blink_time != 0.0f;
    cursor_blink_time = 0.0f;
    cursor_visible = false;
    return changed;
  }

  cursor_blink_time += delta_ms;
  if (cursor_blink_time < period_ms) return false;

  cursor_blink_time = 0.0f;
  cursor_visible = !cursor_visible;
  return true;
}

inline bool copy_selection_to_clipboard(const std::string& text) {
  return !text.empty() && clipboard::write_text(text);
}

inline bool read_clipboard_text(std::string& text) {
  text = clipboard::read_text();
  return !text.empty();
}

template <typename Callback>
inline void notify_change(const std::string& text, Callback&& change_callback) {
  if (change_callback) {
    std::forward<Callback>(change_callback)(text);
  }
}

template <typename Callback>
inline void finish_text_change(
    const std::string& text,
    Callback&& change_callback,
    float& cursor_blink_time,
    bool& cursor_visible,
    bool* dirty = nullptr,
    Element* elem = nullptr) {
  finish_cursor_change(cursor_blink_time, cursor_visible, dirty, elem);
  notify_change(text, std::forward<Callback>(change_callback));
}

template <typename CopyFn>
inline bool handle_copy_shortcut(bool ctrl, KeyCode key, CopyFn&& copy_selection) {
  if (!ctrl || key != KeyCode::C) {
    return false;
  }

  return std::forward<CopyFn>(copy_selection)();
}

template <typename CutFn, typename OnTextChangedFn>
inline bool handle_cut_shortcut(
    bool ctrl,
    KeyCode key,
    bool readonly,
    CutFn&& cut_selection,
    OnTextChangedFn&& on_text_changed) {
  if (!ctrl || key != KeyCode::X) {
    return false;
  }
  if (readonly) {
    return false;
  }

  if (std::forward<CutFn>(cut_selection)()) {
    std::forward<OnTextChangedFn>(on_text_changed)();
  }
  return true;
}

template <typename PasteFn, typename OnTextChangedFn>
inline bool handle_paste_shortcut(
    bool ctrl,
    KeyCode key,
    bool readonly,
    PasteFn&& paste_text,
    OnTextChangedFn&& on_text_changed) {
  if (!ctrl || key != KeyCode::V) {
    return false;
  }
  if (readonly) {
    return false;
  }

  if (std::forward<PasteFn>(paste_text)()) {
    std::forward<OnTextChangedFn>(on_text_changed)();
  }
  return true;
}

template <typename SelectAllFn, typename OnSelectionChangedFn>
inline bool handle_select_all_shortcut(
    bool ctrl,
    KeyCode key,
    SelectAllFn&& select_all,
    OnSelectionChangedFn&& on_selection_changed) {
  if (!ctrl || key != KeyCode::A) {
    return false;
  }

  std::forward<SelectAllFn>(select_all)();
  std::forward<OnSelectionChangedFn>(on_selection_changed)();
  return true;
}

template <typename UndoFn, typename RedoFn, typename OnTextChangedFn>
inline bool handle_undo_redo_shortcut(
    bool ctrl,
    bool shift,
    KeyCode key,
    UndoFn&& undo,
    RedoFn&& redo,
    OnTextChangedFn&& on_text_changed) {
  if (!ctrl) {
    return false;
  }

  switch (key) {
    case KeyCode::Z:
      if (shift) {
        if constexpr (std::is_same_v<std::invoke_result_t<RedoFn>, bool>) {
          if (!std::forward<RedoFn>(redo)()) {
            return false;
          }
        } else {
          std::forward<RedoFn>(redo)();
        }
      } else {
        if constexpr (std::is_same_v<std::invoke_result_t<UndoFn>, bool>) {
          if (!std::forward<UndoFn>(undo)()) {
            return false;
          }
        } else {
          std::forward<UndoFn>(undo)();
        }
      }
      std::forward<OnTextChangedFn>(on_text_changed)();
      return true;

    case KeyCode::Y:
      if constexpr (std::is_same_v<std::invoke_result_t<RedoFn>, bool>) {
        if (!std::forward<RedoFn>(redo)()) {
          return false;
        }
      } else {
        std::forward<RedoFn>(redo)();
      }
      std::forward<OnTextChangedFn>(on_text_changed)();
      return true;

    default:
      return false;
  }
}

template <typename Index>
inline bool has_selection(Index selection_start, Index selection_end) {
  return selection_start >= 0 && selection_end >= 0 && selection_start != selection_end;
}

template <typename Index>
inline void clear_selection(Index& selection_start, Index& selection_end) {
  selection_start = static_cast<Index>(-1);
  selection_end = static_cast<Index>(-1);
}

template <typename Index>
inline void clear_selection_if_needed(
    bool keep_selection, Index& selection_start, Index& selection_end) {
  if (!keep_selection) {
    clear_selection(selection_start, selection_end);
  }
}

template <typename Index>
struct SelectionController {
  Index& selection_anchor;
  Index& selection_start;
  Index& selection_end;

  void clear() {
    selection_anchor = static_cast<Index>(-1);
    clear_selection(selection_start, selection_end);
  }

  void begin(Index anchor) {
    selection_anchor = anchor;
    selection_start = anchor;
    selection_end = anchor;
  }

  void update_active(Index cursor_position) {
    if (selection_anchor < 0) {
      begin(cursor_position);
      return;
    }

    selection_start = selection_anchor;
    selection_end = cursor_position;
  }

  void collapse_if_empty() {
    if (!has_selection(selection_start, selection_end)) {
      clear();
    }
  }

  template <typename MoveFn>
  bool navigate(bool keep_selection, Index& cursor_position, MoveFn&& move_cursor) {
    const Index anchor_before = selection_anchor;
    const Index cursor_before = cursor_position;

    std::forward<MoveFn>(move_cursor)();

    if (!keep_selection) {
      clear();
      return true;
    }

    if (cursor_position == cursor_before) {
      return true;
    }

    if (anchor_before < 0) {
      begin(cursor_before);
    } else {
      selection_start = anchor_before;
      selection_end = cursor_position;
    }

    selection_end = cursor_position;
    collapse_if_empty();
    return true;
  }
};

template <typename MoveFn, typename Index>
inline bool handle_navigation(
    bool keep_selection,
    Index& cursor_position,
    Index& selection_anchor,
    Index& selection_start,
    Index& selection_end,
    MoveFn&& move_cursor) {
  SelectionController<Index> controller{selection_anchor, selection_start, selection_end};
  return controller.navigate(keep_selection, cursor_position, std::forward<MoveFn>(move_cursor));
}

template <typename Index, typename Size>
inline void select_all(Index& selection_start, Index& selection_end, Size text_size) {
  selection_start = 0;
  selection_end = static_cast<Index>(text_size);
}

template <typename Index>
inline void clear_selection_state(
    Index& selection_anchor, Index& selection_start, Index& selection_end) {
  SelectionController<Index> controller{selection_anchor, selection_start, selection_end};
  controller.clear();
}

template <typename Index, typename Size>
inline void select_all(
    Index& selection_anchor, Index& selection_start, Index& selection_end, Size text_size) {
  SelectionController<Index> controller{selection_anchor, selection_start, selection_end};
  controller.begin(0);
  selection_end = static_cast<Index>(text_size);
}

template <typename Index>
inline std::pair<Index, Index> selection_bounds(Index selection_start, Index selection_end) {
  if (selection_start <= selection_end) {
    return {selection_start, selection_end};
  }

  return {selection_end, selection_start};
}

template <typename Index>
inline bool erase_selected_text(
    std::string& text,
    Index& cursor_position,
    Index& selection_anchor,
    Index& selection_start,
    Index& selection_end) {
  if (!has_selection(selection_start, selection_end)) {
    return false;
  }

  const auto [start, end] = selection_bounds(selection_start, selection_end);
  text.erase(static_cast<size_t>(start), static_cast<size_t>(end - start));
  cursor_position = start;
  clear_selection_state(selection_anchor, selection_start, selection_end);
  return true;
}

template <typename Index>
inline bool insert_text_at_cursor(
    std::string& text,
    const std::string& inserted_text,
    Index& cursor_position,
    Index& selection_anchor,
    Index& selection_start,
    Index& selection_end) {
  if (inserted_text.empty()) {
    return false;
  }

  erase_selected_text(text, cursor_position, selection_anchor, selection_start, selection_end);
  text.insert(static_cast<size_t>(cursor_position), inserted_text);
  cursor_position += static_cast<Index>(inserted_text.size());
  return true;
}

template <typename Index>
inline bool erase_char_before_cursor(std::string& text, Index& cursor_position) {
  if (cursor_position <= 0) {
    return false;
  }

  text.erase(static_cast<size_t>(cursor_position - 1), 1);
  --cursor_position;
  return true;
}

template <typename Index>
inline bool erase_char_after_cursor(std::string& text, Index& cursor_position) {
  if (cursor_position >= static_cast<Index>(text.size())) {
    return false;
  }

  text.erase(static_cast<size_t>(cursor_position), 1);
  return true;
}

template <typename Index>
inline void begin_drag_selection(
    Index cursor_position,
    Index& selection_anchor,
    Index& selection_start,
    Index& selection_end,
    bool& is_dragging) {
  SelectionController<Index> controller{selection_anchor, selection_start, selection_end};
  controller.begin(cursor_position);
  is_dragging = true;
}

template <typename Index>
inline void update_drag_selection(
    Index cursor_position,
    Index& selection_anchor,
    Index& selection_start,
    Index& selection_end) {
  SelectionController<Index> controller{selection_anchor, selection_start, selection_end};
  controller.update_active(cursor_position);
}

template <typename Index>
inline void finish_drag_selection(
    Index& selection_anchor, Index& selection_start, Index& selection_end, bool& is_dragging) {
  SelectionController<Index> controller{selection_anchor, selection_start, selection_end};
  is_dragging = false;
  controller.collapse_if_empty();
}

inline void clear_composition_state(bool& is_composing, std::string& composition_text) {
  is_composing = false;
  composition_text.clear();
}

inline bool handle_composition_start(
    bool& is_composing, std::string& composition_text, Element& elem) {
  is_composing = true;
  composition_text.clear();
  elem.mark_paint_dirty();
  return true;
}

inline bool handle_composition_update(
    bool& is_composing, std::string& composition_text, const std::string& text, Element& elem) {
  is_composing = true;
  composition_text = text;
  elem.mark_paint_dirty();
  return true;
}

inline bool handle_composition_end(
    bool& is_composing, std::string& composition_text, Element& elem) {
  clear_composition_state(is_composing, composition_text);
  elem.mark_paint_dirty();
  return true;
}

inline bool handle_focus_out(
    bool& is_composing,
    std::string& composition_text,
    float& cursor_blink_time,
    bool& cursor_visible,
    Element& elem) {
  clear_composition_state(is_composing, composition_text);
  cursor_blink_time = 0.0f;
  cursor_visible = false;
  elem.mark_paint_dirty();
  return true;
}

inline bool handle_focus_in(float& cursor_blink_time, bool& cursor_visible, Element& elem) {
  finish_cursor_change(cursor_blink_time, cursor_visible, nullptr, &elem);
  return true;
}

} // namespace flexUI::text_input_common

#endif // FLEXUI_TEXT_INPUT_COMMON_H

#pragma once

#include "types.h"

#include <vector>

namespace whiteboard {

class ClipboardManager {
public:
  static void copy(const std::vector<Stroke> &strokes, const std::vector<int> &selected_indices) {
    s_clipboard.clear();
    for (int idx : selected_indices) {
      if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
        s_clipboard.push_back(deep_copy_stroke(strokes[static_cast<size_t>(idx)]));
      }
    }
  }

  static void cut(std::vector<Stroke> &strokes, std::vector<int> &selected_indices) {
    copy(strokes, selected_indices);
    // Deletion handled by caller for undo/redo correctness.
  }

  static std::vector<Stroke> paste() {
    std::vector<Stroke> pasted;
    pasted.reserve(s_clipboard.size());
    for (const auto &stroke : s_clipboard) {
      Stroke copy = deep_copy_stroke(stroke);
      copy.move(20.0f, 20.0f);
      copy.selected = false;
      pasted.push_back(copy);
    }
    return pasted;
  }

  static bool has_content() { return !s_clipboard.empty(); }

private:
  static Stroke deep_copy_stroke(const Stroke &stroke) {
    Stroke copy = stroke;
    copy.is_editing = false;
    return copy;
  }

  inline static std::vector<Stroke> s_clipboard;
};

} // namespace whiteboard


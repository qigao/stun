#pragma once

#include <flexUI/widgets/textarea_widget.h>

#include <string>

namespace textedit {

/**
 * flexUI-backed compatibility surface for the legacy ImGui TextEditor.
 *
 * Coordinates use UTF-8 byte columns, matching flexUI's editable text
 * contract. Rendering, selection, caret, sizing, IME, and input are delegated
 * to TextAreaWidget and therefore use ComputedStyle and RenderCommandList.
 */
class TextEditor final : public flexUI::TextAreaWidget {
public:
  struct Coordinates {
    int line = 0;
    int column = 0;

    Coordinates() = default;
    Coordinates(int line_value, int column_value)
        : line(line_value), column(column_value) {}

    bool operator==(const Coordinates& other) const {
      return line == other.line && column == other.column;
    }
  };

  explicit TextEditor(const std::string& text = "",
                      const std::string& placeholder = "")
      : flexUI::TextAreaWidget(text, placeholder) {}

  const char* type_name() const override { return "FlexUITextEditor"; }

  void SetText(const std::string& text) { set_text(text); }
  const std::string& GetText() const { return text(); }
  int GetTotalLines() const { return line_count(); }

  void SetReadOnly(bool value) { set_readonly(value); }
  bool IsReadOnly() const { return is_readonly(); }

  void SetCursorPosition(const Coordinates& position) {
    set_cursor_position(position_from_line_column(position.line, position.column));
  }
  Coordinates GetCursorPosition() const {
    const auto position = line_column_from_position(cursor_position());
    return {position.first, position.second};
  }

  void SetSelection(const Coordinates& start, const Coordinates& end) {
    set_selection(position_from_line_column(start.line, start.column),
                  position_from_line_column(end.line, end.column));
  }
  Coordinates GetSelectionStart() const {
    const auto position = line_column_from_position(selection_start());
    return {position.first, position.second};
  }
  Coordinates GetSelectionEnd() const {
    const auto position = line_column_from_position(selection_end());
    return {position.first, position.second};
  }
  bool HasSelection() const { return has_selection(); }
  std::string GetSelectedText() const { return selected_text(); }
  void ClearSelection() { clear_selection(); }

  void SetScrollOffset(float offset) { set_scroll_offset(offset); }
  float GetScrollOffset() const { return scroll_offset(); }
};

} // namespace textedit

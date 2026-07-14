#include "FlexUITextEditor.h"

#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/render_command.h>
#include <tinytest.h>

#include <cmath>
#include <variant>

using textedit::TextEditor;

namespace {

bool close_color(const flex::Color& actual, const flex::Color& expected,
                 float tolerance = 0.01f) {
  return std::abs(actual.r - expected.r) <= tolerance &&
         std::abs(actual.g - expected.g) <= tolerance &&
         std::abs(actual.b - expected.b) <= tolerance &&
         std::abs(actual.a - expected.a) <= tolerance;
}

struct EditorHarness {
  flexUI::Box box{nullptr};
  flexUI::Element* root = nullptr;
  flexUI::Element* element = nullptr;
  TextEditor* editor = nullptr;

  EditorHarness() {
    root = box.create("div", "root");
    element = box.create_widget<TextEditor>("code-editor", "editor",
                                             "alpha\nbeta");
    editor = static_cast<TextEditor*>(element->widget);
    root->append(element);
    box.set_root(root);
    box.set_viewport(400.0f, 240.0f);
    box.load_css(R"(
      #root { width: 100%; height: 100%; }
      #editor {
        width: 320px;
        height: 160px;
        padding: 8px 12px;
        font-size: 20px;
        --line-height: 1.5;
        --textarea-bg: rgba(10, 18, 30, 1);
        --textarea-text: rgba(220, 230, 240, 1);
        --textarea-selection-bg: rgba(30, 120, 220, 0.75);
        --textarea-cursor: rgba(250, 200, 40, 1);
      }
    )");
    box.update();
    box.set_focus(element);
  }
};

} // namespace

suite("TextEdit flexUI adapter") {
  it("maps legacy line column selection onto flexUI text state") {
    EditorHarness harness;

    harness.editor->SetSelection({0, 1}, {1, 2});

    check(harness.editor->HasSelection());
    check_string_eq(harness.editor->GetSelectedText(), "lpha\nbe");
    check(harness.editor->GetSelectionStart() == TextEditor::Coordinates(0, 1));
    check(harness.editor->GetSelectionEnd() == TextEditor::Coordinates(1, 2));
  }

  it("normalizes byte columns to UTF-8 character boundaries") {
    TextEditor editor("A\xE4\xB8\xAD" "B\nnext");

    editor.SetSelection({0, 2}, {0, 4});

    check(editor.GetSelectionStart() == TextEditor::Coordinates(0, 1));
    check(editor.GetSelectionEnd() == TextEditor::Coordinates(0, 4));
    check_string_eq(editor.GetSelectedText(), "\xE4\xB8\xAD");
  }

  it("reports a themed caret rectangle from flexUI layout metrics") {
    EditorHarness harness;
    harness.editor->SetCursorPosition({1, 2});

    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    harness.editor->get_caret_rect(*harness.element, x, y, width, height);

    check(x > 12.0f);
    check(y >= 38.0f);
    check_float_eq(width, 2.0f, 0.001f);
    check_float_eq(height, 30.0f, 0.001f);
  }

  it("measures intrinsic size from text font line height and padding") {
    EditorHarness harness;
    float width = 0.0f;
    float height = 0.0f;

    const bool measured = harness.editor->measure_intrinsic_size(
        *harness.element, 400.0f, 240.0f, width, height);

    check(measured);
    check(width >= 200.0f);
    check_float_eq(height, 76.0f, 0.001f);
  }

  it("emits selection and caret rectangles through render commands") {
    EditorHarness harness;
    harness.editor->SetSelection({0, 1}, {0, 4});

    flexUI::RenderCommandList commands(flex::RendererCapabilities{});
    harness.editor->emit_render_commands(*harness.element, commands);

    size_t rect_count = 0;
    bool found_selection = false;
    bool found_caret = false;
    for (const auto& command : commands.commands()) {
      if (std::holds_alternative<flexUI::DrawRectCommand>(command)) {
        ++rect_count;
        const auto& rect = std::get<flexUI::DrawRectCommand>(command);
        found_selection = found_selection || close_color(
            rect.fill.color, flex::Color{30.0f / 255.0f, 120.0f / 255.0f,
                                         220.0f / 255.0f, 0.75f});
        found_caret = found_caret || close_color(
            rect.fill.color, flex::Color{250.0f / 255.0f, 200.0f / 255.0f,
                                         40.0f / 255.0f, 1.0f});
      }
    }
    check_size_ge(rect_count, 3);
    check(found_selection);
    check(found_caret);
  }
}

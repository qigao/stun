/*
 * Text Tool
 *
 * Tool for creating and editing text nodes.
 * Click to place text, type to edit.
 */

#pragma once

#include "../viewmodel/tool.h"
#include "../viewmodel/editor_vm.h"
#include "../command/command.h"
#include "draw_tool.h"

namespace editor {

class TextTool : public Tool {
public:
    const char* name() const override { return "Text"; }
    const char* icon() const override { return "text"; }
    const char* tooltip() const override { return "Create text (T)"; }

    void activate(EditorViewModel* vm) override;
    void deactivate() override;

    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseUp(const MouseEvent& e) override;
    bool onKeyDown(const KeyEvent& e) override;

    void render(flex::Renderer& renderer) override;

private:
    void finishEditing();
    void createTextNode();

    bool is_editing_ = false;
    Point text_position_;
    std::string text_content_;

    // Text properties
    std::string font_family_ = "Arial";
    float font_size_ = 24.0f;
    Color text_color_ = {0, 0, 0, 1};
};

} // namespace editor

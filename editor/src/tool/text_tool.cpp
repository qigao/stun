/*
 * Text Tool Implementation
 */

#include <editor/tool/text_tool.h>
#include <editor/model/node.h>

namespace editor {

void TextTool::activate(EditorViewModel* vm) {
    Tool::activate(vm);
    is_editing_ = false;
    text_content_.clear();
}

void TextTool::deactivate() {
    finishEditing();
    Tool::deactivate();
}

bool TextTool::onMouseDown(const MouseEvent& e) {
    if (e.button != 0) return false;

    // If already editing, finish current text and start new
    if (is_editing_) {
        finishEditing();
    }

    // Start editing at click position
    is_editing_ = true;
    text_position_ = e.position;
    text_content_.clear();

    return true;
}

bool TextTool::onMouseUp(const MouseEvent& e) {
    (void)e;
    return false;
}

bool TextTool::onKeyDown(const KeyEvent& e) {
    if (!is_editing_) return false;

    // Escape to cancel
    if (e.key == 27) {
        is_editing_ = false;
        text_content_.clear();
        return true;
    }

    // Enter to finish
    if (e.key == 13) {
        finishEditing();
        return true;
    }

    // Backspace
    if (e.key == 8) {
        if (!text_content_.empty()) {
            text_content_.pop_back();
        }
        return true;
    }

    // Regular character input (printable ASCII)
    if (e.key >= 32 && e.key < 127) {
        text_content_ += static_cast<char>(e.key);
        return true;
    }

    return false;
}

void TextTool::render(flex::Renderer& renderer) {
    if (!is_editing_) return;

    float viewScale = vm_->camera().screenToWorldScale();

    // Draw text cursor/caret
    std::string displayText = text_content_;
    if (displayText.empty()) {
        displayText = "Type here...";
    }

    // Apply transform to text position (same as Text::render does)
    renderer.save();
    renderer.translate(text_position_.x, text_position_.y);

    // Draw text preview at local origin (0, 0)
    renderer.draw_text(displayText, 0, 0,
                       font_family_, font_size_, false,
                       {text_color_.r, text_color_.g, text_color_.b,
                        text_content_.empty() ? 0.4f : text_color_.a});

    // Draw blinking cursor (simple line) in local coordinates
    float cursorX = 0;
    if (!text_content_.empty()) {
        // Approximate cursor position based on character count
        cursorX = text_content_.length() * font_size_ * 0.5f;
    }

    std::string cursorPath = "M " + std::to_string(cursorX) + " " + std::to_string(-font_size_) +
        " v " + std::to_string(font_size_ * 1.2f);
    renderer.stroke_path(cursorPath, flex::Paint::solid({0, 0, 0, 1}), viewScale * 2);

    // Draw text bounds indicator in local coordinates
    float textWidth = text_content_.empty() ? 100 : text_content_.length() * font_size_ * 0.5f;
    std::string boundsPath = "M " + std::to_string(-2) + " " + std::to_string(-font_size_ - 2) +
        " h " + std::to_string(textWidth + 4) +
        " v " + std::to_string(font_size_ * 1.2f + 4) +
        " h " + std::to_string(-(textWidth + 4)) + " Z";
    renderer.stroke_path(boundsPath, flex::Paint::solid({0.2f, 0.5f, 1.0f, 0.5f}), viewScale);

    renderer.restore();
}

void TextTool::finishEditing() {
    if (!is_editing_) return;

    if (!text_content_.empty()) {
        createTextNode();
    }

    is_editing_ = false;
    text_content_.clear();
}

void TextTool::createTextNode() {
    auto text = flex::Text::create();
    text->set_position(text_position_.x, text_position_.y);
    text->set_content(text_content_);
    text->set_font_family(font_family_);
    text->set_font_size(font_size_);
    text->set_color(flex::Color{text_color_.r, text_color_.g, text_color_.b, text_color_.a});

    auto node = TextNode::create(text);
    node->setName("Text");
    vm_->executeCommand(std::make_unique<CreateNodeCommand>(vm_, node));
}

} // namespace editor

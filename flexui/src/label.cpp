#include <flexui/label.h>
#include <cssbox_internal.h>

namespace flexui {

Label::Label(cssboxRenderer* renderer, const std::string& id, const std::string& text,
             const LabelStyle& style)
    : Widget(renderer, id, "label"), text_(text), style_(style) {
    // Set text_content for layout engine to calculate intrinsic size
    element()->text_content = text;
}

void Label::setLabelText(const std::string& text) {
    text_ = text;
    element()->text_content = text;
}

void Label::draw(NVGcontext* vg) {
    // REMOVED: cssbox already renders text_content via paint_text_content()
    // The text is set in constructor: element()->text_content = text;
}

} // namespace flexui

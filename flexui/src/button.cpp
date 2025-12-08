#include <flexui/button.h>
#include <cssbox_internal.h>

namespace flexui {

Button::Button(cssboxRenderer *renderer, const std::string &id, const std::string &text,
               const ButtonStyle &style)
    : Widget(renderer, id, "button"), text_(text), style_(style) {
    // Set text_content for layout engine to calculate intrinsic size
    element()->text_content = text;
}

void Button::setButtonText(const std::string& text) {
    text_ = text;
    element()->text_content = text;
}

void Button::draw(NVGcontext *vg) {
    // REMOVED: cssbox renders background + text_content
    // Style with CSS: button { background: blue; } button:hover { background: darkblue; }
}

} // namespace flexui

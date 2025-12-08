#include <flexui/divider.h>
#include <cssbox_internal.h>

namespace flexui {

Divider::Divider(cssboxRenderer* renderer, const std::string& id,
                 bool vertical, const DividerStyle& style)
    : Widget(renderer, id, "hr"), style_(style) {
    style_.vertical = vertical;
}

void Divider::draw(NVGcontext* vg) {
    // REMOVED: cssbox renders hr elements via CSS
    // Style with CSS: hr { border: none; border-top: 1px solid #e0e0e0; height: 0; }
}

} // namespace flexui

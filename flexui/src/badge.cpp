#include <flexui/badge.h>
#include <cssbox_internal.h>
namespace flexui {

Badge::Badge(cssboxRenderer* renderer, const std::string& id, const std::string& text,
             const BadgeStyle& style)
    : Widget(renderer, id, "badge"), text_(text), style_(style) {
    // Set text_content for layout engine to calculate intrinsic size
    element()->text_content = text;
}

void Badge::draw(NVGcontext* vg) {
    // REMOVED: cssbox already renders background + text_content
}

} // namespace flexui

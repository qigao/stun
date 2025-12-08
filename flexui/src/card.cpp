#include <flexui/card.h>
#include <cssbox_internal.h>
namespace flexui {

Card::Card(cssboxRenderer* renderer, const std::string& id, const CardStyle& style)
    : Widget(renderer, id, "card"), style_(style) {
}

void Card::draw(NVGcontext* vg) {
    // REMOVED: cssbox already renders:
    // - background via paint_background()
    // - border-radius via rounded rect
    // - box-shadow via paint_box_shadows()
    // Style these with CSS: .card { background: white; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
}

} // namespace flexui

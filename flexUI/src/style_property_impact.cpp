#include <flexUI/detail/style_property_registry.h>
#include <flexUI/element.h>

namespace flexUI::detail {

void mark_style_impact(Element& element, std::uint32_t impact) noexcept {
    const bool text_layout =
        (impact & STYLE_IMPACT_TEXT_LAYOUT) != 0u;
    const bool layout =
        text_layout || (impact & STYLE_IMPACT_LAYOUT) != 0u;
    const bool hit_test =
        (impact & STYLE_IMPACT_HIT_TEST) != 0u;
    const bool visual =
        (impact & (STYLE_IMPACT_PAINT | STYLE_IMPACT_COMPOSITE)) != 0u;

    // Text measurement changes invalidate content-derived bounds in addition
    // to layout. mark_layout_dirty() owns the Box layout + paint notification.
    if (text_layout) {
        element.mark_dirty(flex::DirtyFlags::Content |
                           flex::DirtyFlags::Bounds);
    }

    if (layout) {
        element.mark_layout_dirty();
    }

    // Hit-test changes need fresh bounds/world-bounds even when they do not
    // participate in layout (for example transform-family properties).
    if (hit_test) {
        element.mark_dirty(flex::DirtyFlags::Bounds);
    }

    // Keep Visual explicit. mark_layout_dirty() notifies Box painting but does
    // not itself set the Element Visual bit.
    if (visual || hit_test) {
        element.mark_paint_dirty();
    }
}


} // namespace flexUI::detail

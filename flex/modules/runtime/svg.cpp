/*
 * Flex Engine - SVG Node Implementation
 */

#include "flex/core/svg.h"
#include "flex/core/renderer.h"

namespace flex {

void Svg::render(Renderer& renderer) {
    if (!visible()) return;
    if (src_.empty() && data_.empty()) return;

    const float render_width = width_ > 0.0f ? width_ : layout_width();
    const float render_height = height_ > 0.0f ? height_ : layout_height();

    renderer.save();

    if (position_mode() == PositionMode::Fixed) {
        renderer.reset_clip();
        renderer.set_transform(world_transform());
    } else {
        renderer.translate(x(), y());
        renderer.rotate(rotation());
        renderer.scale(scale_x(), scale_y());
    }
    renderer.set_global_alpha(opacity());
    if (has_shadow()) renderer.set_shadow(shadow());
    if (has_blur()) renderer.set_blur(blur());

    if (!data_.empty()) {
        renderer.draw_svg_data(data_, 0, 0, render_width, render_height);
    } else {
        renderer.draw_svg(src_, 0, 0, render_width, render_height);
    }

    if (has_shadow()) renderer.clear_shadow();
    if (has_blur()) renderer.clear_blur();

    renderer.restore();
}
} // namespace flex

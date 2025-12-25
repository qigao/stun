/*
 * Flex Engine - SVG Node Implementation
 */

#include "flex/runtime/svg.h"
#include "flex/runtime/renderer.h"

namespace flex {

void Svg::render(Renderer& renderer) {
    if (!visible()) return;
    if (src_.empty() && data_.empty()) return;

    renderer.save();
    renderer.translate(x(), y());
    renderer.rotate(rotation());
    renderer.scale(scale_x(), scale_y());
    renderer.set_global_alpha(opacity());

    if (!data_.empty()) {
        renderer.draw_svg_data(data_, 0, 0, width_, height_);
    } else {
        renderer.draw_svg(src_, 0, 0, width_, height_);
    }

    renderer.restore();
}
} // namespace flex

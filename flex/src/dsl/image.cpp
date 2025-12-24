/*
 * Flex Engine - Image Node Implementation
 */

#include "flex/dsl/image.h"
#include "flex/renderer.h"

namespace flex {

void Image::render(Renderer& renderer) {
    if (!visible() || src_.empty()) return;

    renderer.save();
    renderer.translate(x(), y());
    renderer.rotate(rotation());
    renderer.scale(scale_x(), scale_y());
    renderer.set_global_alpha(opacity());

    renderer.draw_image(src_, 0, 0, width_, height_);

    renderer.restore();
}

} // namespace flex

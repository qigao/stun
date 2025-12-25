/*
 * Flex Engine - Image Node Implementation
 */

#include "flex/dsl/image.h"
#include "flex/renderer.h"

namespace flex {

Bounds Image::compute_bounds() const {
    return Bounds{0, 0, width_, height_};
}

void Image::render(Renderer& renderer) {
    if (!visible() || src_.empty()) return;

    renderer.save();
    renderer.set_transform(world_transform());
    renderer.set_global_alpha(opacity());

    renderer.draw_image(src_, 0, 0, width_, height_);

    renderer.restore();
}

} // namespace flex

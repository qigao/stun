/*
 * Flex Engine - Image Node Implementation
 */

#include "flex/runtime/image.h"
#include "flex/runtime/renderer.h"

namespace flex {

Bounds Image::compute_bounds() const {
    return Bounds{0, 0, width_, height_};
}

void Image::render(Renderer& renderer) {
    if (!visible() || src_.empty()) return;

    renderer.save();
    
    // Apply local transform (relative to parent, accumulated by renderer stack)
    renderer.translate(x(), y());
    if (rotation() != 0.0f) {
        renderer.rotate(rotation());
    }
    if (scale_x() != 1.0f || scale_y() != 1.0f) {
        renderer.scale(scale_x(), scale_y());
    }
    
    renderer.set_global_alpha(opacity());

    renderer.draw_image(src_, 0, 0, width_, height_);

    renderer.restore();
}

} // namespace flex

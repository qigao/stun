/*
 * Flex Engine - Image Node Implementation
 */

#include "flex/core/image.h"
#include "flex/core/renderer.h"

namespace flex {

Bounds Image::compute_bounds() const {
    return Bounds{0, 0, width_, height_};
}

void Image::render(Renderer& renderer) {
    if (!visible() || src_.empty()) return;

    renderer.save();

    if (position_mode() == PositionMode::Fixed) {
        renderer.reset_clip();
        renderer.set_transform(world_transform());
    } else {
        // Apply local transform (relative to parent, accumulated by renderer stack)
        renderer.translate(x(), y());
        if (rotation() != 0.0f) {
            renderer.rotate(rotation());
        }
        if (scale_x() != 1.0f || scale_y() != 1.0f) {
            renderer.scale(scale_x(), scale_y());
        }
    }
    
    renderer.set_global_alpha(opacity());

    if (has_shadow()) renderer.set_shadow(shadow());
    if (has_blur()) renderer.set_blur(blur());

    renderer.draw_image(src_, 0, 0, width_, height_);

    if (has_shadow()) renderer.clear_shadow();
    if (has_blur()) renderer.clear_blur();

    renderer.restore();
}

} // namespace flex

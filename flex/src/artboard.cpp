/*
 * Flex Engine - Artboard Implementation
 */

#include "flex/artboard.h"
#include "flex/renderer.h"

namespace flex {

Artboard::Artboard(float width, float height)
    : width_(width)
    , height_(height)
    , root_(Group::create())
{
    root_->set_id("root");
}

void Artboard::render(Renderer& renderer) {
    // Clear with background color
    renderer.clear(background_);

    // Render scene graph
    root_->render(renderer);
}

} // namespace flex

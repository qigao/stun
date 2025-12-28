/*
 * Flex Engine - Scene Implementation
 */

#include "flex/runtime/scene.h"
#include "flex/runtime/renderer.h"

namespace flex {

Scene::Scene(float width, float height, ArenaAllocator& arena)
    : width_(width)
    , height_(height)
    , root_(Group::create(arena))
{
    root_->set_id("root");
}

void Scene::render(Renderer& renderer) {
    // Note: Don't clear here - parent (flexUI or demo) handles clearing
    // This allows Scene to be rendered as part of a larger UI

    // Render scene graph
    root_->render(renderer);
}

} // namespace flex

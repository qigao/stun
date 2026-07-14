#pragma once

#include "flex/core/node.h"
#include "flex/core/scene.h"
#include <unordered_map>

namespace flex {
namespace runtime {

Scene::RawPtr clone_scene(
    const Scene *source,
    ArenaAllocator &arena,
    std::unordered_map<Node::RawPtr, Node::SharedPtr> *shared_node_index = nullptr);

} // namespace runtime
} // namespace flex

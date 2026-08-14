/* Internal gCanvas engine shared by OpenGL and Vulkan validation hosts. */
#pragma once

#include "flex/core/renderer.h"

#include <gcanvas/backends/opengl.hpp>

#include <memory>

namespace gcanvas {
class Context;
}

namespace flex::render::engines::gcanvas {

/**
 * Adapts a borrowed gCanvas context to the Flex immediate renderer contract.
 * The context, its presentation host, and all resources it owns must outlive the renderer.
 * end_frame() submits drawing; the host remains responsible for present_frame().
 * Outer and inset shadows cover primitives, arbitrary paths, text, images, and SVG, including
 * signed spread and affine path routing. Sampled work is bounded by the gCanvas resource limits.
 */
std::unique_ptr<Renderer> create_renderer(::gcanvas::Context& context);

/**
 * Owns a gCanvas context that draws into the caller's current OpenGL framebuffer.
 * The caller owns context activation and buffer swapping. proc_loader_user_data must
 * remain valid until the renderer is destroyed.
 */
std::unique_ptr<Renderer> create_external_opengl_renderer(
    ::gcanvas::opengl::ProcLoader proc_loader, void* proc_loader_user_data = nullptr);

} // namespace flex::render::engines::gcanvas

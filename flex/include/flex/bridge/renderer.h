/*
 * Flex Engine - Renderer Factory Registry
 *
 * Public backend-neutral renderer registration and creation API.
 */

#pragma once

#include "flex/core/renderer.h"

#include <memory>

namespace flex {

enum class RendererBackend {
  OpenGL,
  Vulkan,
  TUI,
  Direct2D,
  // Keep Custom last; the registry uses it as the fixed backend count.
  Custom
};

struct RendererCreateInfo {
  RendererBackend backend = RendererBackend::Custom;
  CanvasHandle handle = nullptr;
};

using RendererFactory = std::unique_ptr<Renderer> (*)(CanvasHandle handle);

const char *renderer_backend_name(RendererBackend backend);
RendererFactory renderer_backend_factory(RendererBackend backend);
RendererFactory default_renderer_factory();
RendererFactory set_default_renderer_factory(RendererFactory factory);
bool set_default_renderer_backend(RendererBackend backend);

// Register or replace a backend factory. Passing nullptr clears the backend.
// The previous factory is returned so callers can restore temporary overrides.
RendererFactory register_renderer_backend(RendererBackend backend, RendererFactory factory);
bool is_renderer_backend_available(RendererBackend backend);

// Backend-agnostic entry point intended for end users after application bootstrap
// has selected a default renderer plugin/factory.
std::unique_ptr<Renderer> create_renderer(CanvasHandle handle);
std::unique_ptr<Renderer> create_renderer(RendererBackend backend, CanvasHandle handle);
std::unique_ptr<Renderer> create_renderer(const RendererCreateInfo &info);

} // namespace flex

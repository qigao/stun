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
  Custom,
  // Explicit Tango OpenGL cell-grid presenter; no production factory is
  // installed until the native GL host and GPU glyph/pipeline path qualify.
  // Appended after Custom so existing public enum ordinals are unchanged.
  TangoOpenGL
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

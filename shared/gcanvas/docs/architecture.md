# gCanvas renderer-core decision

## Context

The upstream project mixed a canvas renderer, GLFW window ownership, UI/layout experiments, media
features, and several compile-time backend branches. Flex already owns widget layout and event
semantics, so importing the whole framework would create two competing facts for layout, focus,
and widget lifetime.

This fork therefore treats gCanvas as a rendering dependency. The stable concept is a
canvas; OpenGL and Vulkan are interchangeable implementations; a window is presentation support
rather than UI policy.

## Options considered

1. Import upstream unchanged. This has the lowest initial migration cost but keeps duplicate UI
   state, backend-dependent symbols, and compile-time backend selection.
2. Extract only Vulkan. This is smaller, but it creates a Vulkan-specific abstraction and cannot
   validate that the canvas contract is actually backend-neutral.
3. Split a canvas core from OpenGL, Vulkan, and window targets. This costs more now but gives both
   renderers one public contract and lets Flex remain the only widget/layout layer.

Option 3 is selected.

## Current boundary

`gcanvas::Context` owns drawing state and GPU registration state for exactly one `gcanvas::Backend`.
`Image` and `Font` handles are created by that context so a resource from one backend cannot be
constructed through another backend's global factory. The context owns every returned image and
font, while `Font` owns its atlas; callers receive borrowed references and perform no explicit
destruction.

`Window` owns the GLFW window and, in compatibility mode, the context returned by
`Window::create_context()`. Destruction releases the context while its native window/context is
still valid, then destroys cursors and the GLFW window. Backend shared state is reference-counted
by renderer contexts; the last window terminates GLFW.

The implementation is single-threaded. A window, its context, and its resources must be created,
used, resized, and destroyed on the presentation thread. There is no implicit backend fallback:
the requested backend either initializes successfully or reports the existing initialization
failure.

## Targets and dependencies

- `Core` contains no renderer selection and defines the canvas/resource contracts.
- `OpenGL` and `Vulkan` implement those contracts and may be linked into the same process.
- `Window` provides runtime selection among the renderer targets enabled for that build.
- Flex is expected to adapt its paint commands to `Context`; gCanvas does not own Flex
  nodes, focus, input dispatch, style resolution, or layout.

The public drawing names are retained. Static resource factories and compile-time backend macros
are an intentional source migration: callers use `context.create_image/create_font` and select
`WindowConfig::backend`. Unsupported warning-only operations were removed rather than pretending
to succeed.

## Host-injection boundary

Version 0.4 removes the renderer-to-`Window` dependency without changing the canvas drawing
surface:

- OpenGL receives procedure loading, make-current, framebuffer extent, swap interval, and swap
  callbacks.
- Vulkan receives required instance-extension and native-surface callbacks. The renderer owns the
  Vulkan instance, device, queue work, swapchain, readback, and presentation.
- `CanvasMetrics` is copied state and is the sole renderer-visible source for logical size, scale,
  offset, and DPI. The backend render-target extent is separate physical state updated only by
  `resize_context()` or the external target callback; changing logical metrics cannot overwrite it.

The create-info and callback tables are thin platform adapters. They contain no Flex node, input,
or layout types, so NanoGUI/ThorVG may remain integration helpers without becoming canvas
backends. Adding another window system requires a host adapter, not another renderer API.

## NanoVG reference boundary

The vendored NanoVG implementation is a feature reference, not a runtime dependency of `Core`.
Its path command stream/tessellation, paint and gradient model, affine transform handling,
scissoring, image patterns, and text metrics provide concrete behavior to compare while those
capabilities are added to gCanvas. Implementations remain behind `gcanvas::Context`; shared public
types, backend selection, resource ownership, and error semantics continue to belong to gCanvas.
OpenGL and Vulkan must gain each capability together and pass equivalent contract/pixel tests.
Code adapted from NanoVG must preserve its provenance and license notice.

### Shared path pipeline

The path pipeline shares commands, adaptive cubic flattening, bounded mesh generation, paint
lookup generation, and affine transforms in `Core`. Each backend uploads the same triangle/quad
stream, builds an even-odd fill or union stroke stencil mask, and shades a cover quad. This keeps
one geometry fact source while the GPU owns rasterization, stencil coverage, blending, and texture
sampling. It intentionally does not require OpenGL/Vulkan tessellation shaders.

Solid paths allocate no image and opaque image patterns reuse the source sampler. Gradients and
translucent patterns use fixed square lookup textures, bounded by `path_paint_texture_size` and
`max_path_surfaces`, instead of framebuffer-sized RGBA uploads.
Geometry is bounded by `max_path_mesh_quads`; adaptive curve flattening is additionally bounded by
the public path command limit and recursion depth. All limit, singular-transform, stencil-format,
and stencil-buffer failures are explicit. State is single-threaded and transient texture reuse
advances once per path draw, then resets after present.

OpenGL records path operations in its existing ordered command chain and requires a stencil
default framebuffer. Vulkan owns one stencil attachment per swapchain image and switches among
normal, even-odd mask, union mask, and stencil-tested paint pipelines without changing the public
API. Rollback remains localized to path dispatch and these backend pipeline resources.

### Source-color blur pipeline

Version 0.8 adds draw-scoped blur across Core, OpenGL, Vulkan, and the Flex adapter. Three designs
were evaluated:

1. Full-canvas ping-pong surfaces give a conventional separable postprocess, but they can include
   previously drawn nodes and require persistent framebuffer/image, resize, Vulkan layout, and
   external-target ownership protocols.
2. Backend-local filter implementations avoid a core change, but duplicate kernel, alpha,
   resource-limit, and error semantics.
3. A Core-owned bounded Gaussian tap group keeps `BlurFilter` draw-scoped and gives both backends
   the same ordered commands without a new GPU resource lifetime.

Option 3 is selected. Core is the only kernel and source-over normalization fact source. Path blur
tessellates once, acquires at most one existing transient paint texture, and emits shifted
stencil/paint commands; text and image blur reuse their existing glyph/source textures. The
context frame command vectors remain single-thread owned and follow the existing empty →
recording → consumed → empty lifecycle. Blur adds no framebuffer, image-layout transition,
descriptor set, resize work, or teardown dependency.

`max_blur_samples` bounds taps and `max_blur_commands` bounds `(path_quads + 1) * taps` for one
path draw. The implementation validates counts and reserves every affected command vector before
acquiring a paint texture or appending the blur group. The performance cost is `O(Q * S)` draw
instances and command memory, while paint tessellation/rasterization stays `O(Q)` per call.
Command vectors retain their backend-bounded frame high-water capacity and use 1.5x geometric
growth. Allocation can therefore fail only before the current blur group is appended, without the
quadratic copying behavior caused by exact `size + required` reservations across many paths.
Rolling back source-color blur requires only disabling the adapter capability and routing content
to the existing non-blurred draws; no persistent data or GPU resource migration is involved.

## Validation boundary

Contract tests verify that `Context` is an abstract, non-copyable polymorphic API and that backend
selection is explicit. A dual-backend link test prevents duplicate backend symbols and missing
loader implementations. Build/install consumer tests verify the exported target names and public
headers. Real-GPU tests for both backends render/read back known pixels, resize and recreate the
surface path, validate multi-stop linear/radial gradients, affine paths, ellipse/line/cubic
fill/stroke and image patterns, create and update a dynamic image, and present. Device-loss
injection and resize storms remain broader production-hardening work rather than part of the
library boundary contract.

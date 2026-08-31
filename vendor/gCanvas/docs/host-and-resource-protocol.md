# Host and resource protocol

This document is normative for the gCanvas 0.8 renderer boundary.

## Thread and state model

Each `Context` is single-thread confined. The thread that creates it is the only thread allowed to
update metrics, create resources, record drawing operations, resize, read pixels, present, or
destroy it. Host callbacks execute synchronously on that same thread. No callback may retain a
pointer supplied by gCanvas.

`Window` listener registration, delivery, removal, and destruction are confined to that same owner
thread. Legacy `add_*_listener()` callbacks persist until `reset_listener()` or window destruction.
The `subscribe_*_listener()` variants return a move-only `WindowListenerSubscription`; destroying
or resetting it removes only its callback, and destroying it after the window is safe. During
delivery, removing a callback that has not run prevents its delivery for the current event, while a
new callback starts with the next event. `reset_listener()` clears every category and invalidates
all scoped subscriptions.

Window focus and native close-request callbacks are notifications. They cannot veto closing, and
calling `Window::close()` directly only sets the close flag; it does not synthesize another close
notification. On Windows, `Window` exposes native GUI pointer capture through
`supports_pointer_capture()`, `has_pointer_capture()`, and `set_pointer_capture()`. Acquisition and
release are idempotent for the owning window and fail explicitly if the requested native state was
not reached. Other platforms report no capability and reject capture changes instead of replacing
capture with cursor confinement.

Before publishing a Windows focus-loss notification, `Window` releases any native pointer capture
owned by that window. A host handles the notification by clearing its own UI focus and logical
capture state. Focus gain does not restore either state automatically. Native capture is also
released during window destruction; this cleanup does not change listener ownership.

The frame state is:

```text
idle -> recording -> submitted -> presented -> idle
                       |              ^
                       +-> readback --+
```

`draw_frame()` submits recorded work. `read_pixels()` is valid after submission and before or
after presentation; it waits for the submitted work when required. `present_frame()` completes the
frame. Resize and destruction require the context to be idle or synchronously wait for GPU work.

The window-owned Vulkan path uses two bounded frame-resource slots. Each slot owns one persistently
mapped VMA storage buffer, descriptor set, acquire/render semaphores, and completion fence. A slot
is immutable from queue submission until its fence signals, and a swapchain image is not
re-recorded until the fence of its previous submission signals. CPU draw commands remain immediate
and are cleared after submission; the frame ring retains GPU transport resources, not scene state.
Each storage buffer holds at most 131,072 128-byte draw records, so the window-owned Vulkan frame
ring reserves at most 32 MiB for draw storage. An externally submitted Vulkan context has one such
slot because synchronization and command-buffer lifetime belong to its host.

## Host ownership

An OpenGL or Vulkan create-info is copied into the context. Its `user_data` is borrowed: the host
owns the pointed-to object and keeps it alive until the context destructor returns. Callback
function pointers remain valid for the same interval. Vulkan extension names are copied before the
factory returns and need only remain valid during that callback.

OpenGL `HostManaged` callbacks provide procedure loading, make-current, framebuffer extent, and
buffer swap. `External` presentation requires only procedure loading and framebuffer extent: the
caller keeps the context current and owns buffer swapping, while `present_frame()` still closes
the submitted frame and releases its transient command references.
Vulkan callbacks provide required instance extensions and surface creation. gCanvas owns the
Vulkan instance, device, swapchain, queue submission, and presentation; the host owns the native
window and destroys it only after the context.

Missing callbacks, invalid dimensions, a surface creation error, unsupported presentation, or an
unavailable backend throws during construction. There is no backend substitution or dummy host.

## Resource ownership

`std::unique_ptr<Context>` is the sole public context owner. A `Window` stores its context in a
`std::unique_ptr`; a direct host stores the returned pointer itself.

`Context::create_image()` and `Context::create_font()` return borrowed references. The creating
context is the sole owner and destroys all fonts and images before releasing backend shared state.
References become invalid when the context is destroyed. A resource is valid only with its
creating context; cross-context use is rejected.

A `Font` solely owns its texture atlas. Image constructors copy input pixels into owned storage, so
the caller's file buffer or pixel span may be released when creation returns. Memory-backed image
creation requires an exact byte length and rejects null, truncated, oversized, overflowed, or
invalid-dimension input before GPU registration. There is no global image index and no explicit
`destroy()` operation.

The default limits are 256 images and 32 fonts per context. Creation at the limit throws and leaves
ownership with the context unchanged. Backend descriptor/sampler limits may reduce the effective
image capacity and are checked before GPU registration.

Path commands are flattened into context-owned transient mesh data for the current frame. The
default geometry limit is 262,144 mask quads per path. Non-solid paint lookup textures are 256 x
256 RGBA pixels. Gradient entries use a Context-owned cache bounded by `max_path_surfaces`; an
entry referenced by the current frame is pinned until `present_frame()`, cache hits reuse the same
GPU image, and a miss fails explicitly if every slot is pinned. Opaque image patterns reuse their
existing context-owned sampler. OpenGL borrows the host default framebuffer stencil attachment and
requires at least one bit. Vulkan owns one stencil image, memory allocation, and view per swapchain
image and destroys them with the corresponding framebuffers.

## Shutdown order

1. Stop native event delivery, release native pointer capture, and destroy owner-scoped listener
   subscriptions.
2. Stop recording and wait for submitted GPU work.
3. Destroy context-owned fonts, atlases, images, and dummy resources.
4. Destroy per-context pipelines, buffers, swapchain, and surface.
5. Release the backend shared instance/device reference; the last context destroys shared state.
6. Destroy the native window and terminate the window system after its last window.

Construction failures unwind the successfully acquired prefix in reverse order through RAII.
There is no partially usable public context.

## Validation

Contract tests check non-copyability, unique ownership, callback and pointer-capture signatures,
default resource limits, and the absence of explicit destroy APIs. GPU tests create an invisible
real GLFW window for each built backend, acquire and release native capture on Windows, render and
read back known pixels, resize, create and update a dynamic image, render again, and present. The
OpenGL/Vulkan libraries are also inspected through their exported link interfaces to ensure GLFW
is confined to `gCanvas::Window`.

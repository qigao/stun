# Tango dual presentation: native TTY and OpenGL

Status: **decision + CPU API/admission boundary**; the native OpenGL Tango
renderer has **not** been implemented or qualified. Tracked by
[Stun #17](https://github.com/qigao/stun/issues/17). This design supersedes
statements that *all Tango renderers* lack shaders. It does not enable a GPU
feature merely by declaring a new API value.

## One Tango UI and cell model, two explicitly selected presenters

```text
XML + native Tailwind + TurboScript / Tango C11 input & model
                              |
                   one cell-layout contract
                    validated tui_buffer_t
                    (UTF-8/grapheme/span,
                    fg/bg/style/scroll)
                              |
                    bounded frame snapshot
                         /         \
         Tango TTY presenter       Tango OpenGL presenter
       stdout/VT/TTY/ConPTY       host-owned GL context/window
            cell diffs            glyph atlas + cell instances
          no GPU/shader           GL shader/pipeline + GPU frame
           terminal IO            pixel framebuffer/display
```

The second path is **a Tango terminal-grid compositor backed by OpenGL**,
not an alias for the generic Flex `RendererBackend::OpenGL` and not a second
UI tree. Both presentations preserve Tango's single canonical Unicode/grapheme
policy, wide-cell continuation, control filtering, text attributes, scroll,
selection and cursor semantics. They may differ in *rendered pixels*.

### Public selection and current availability

| Explicit selection | Semantic domain | Presentation | Status |
| --- | --- | --- | --- |
| `RendererBackend::TUI` / effect `BackendApi::Tango` | TerminalCell | native TTY/VT/ConPTY, or borrowed headless cell buffer | **Existing implementation** |
| `RendererBackend::TangoOpenGL` / effect `BackendApi::TangoOpenGL` | Tango cell-grid source; qualified NativePixel output stage | OpenGL context, glyph atlas and GPU pixel presentation | **Selection and effect metadata only; no registered production renderer** |
| `RendererBackend::OpenGL` / effect `BackendApi::OpenGL` | native Flex pixel UI | existing generic gCanvas OpenGL engine | **Different backend and identity** |

The existing TUI and Tango enum ordinals and their physical-terminal
behavior remain unchanged. `TangoOpenGL` has a distinct registration
slot, but until there is an audited native implementation its factory is
null and explicit creation must fail. Neither path can silently fall back
to the other or to generic gCanvas OpenGL. A renderer switch requires
an explicit new host/presenter selection and fresh admission.

## First shared cell-frame implementation

The C11 core now provides `tui_grid_snapshot_capture()` in
`tui_grid_snapshot.h` / `tui_grid_snapshot.c`:
a bounded, owned, versioned deep copy of `tui_buffer_t` cells. The API
takes explicit cell/byte limits and a nonzero caller-owned revision. It
normalizes every leading cell through Tango's existing
`tui_buffer_set()` Unicode/span policy on a stack scratch grid, and
requires matching wide-cell continuations. Invalid metadata, orphan
continuations, inconsistent styles and malformed grapheme data fail before
publishing a pointer. Nothing in the new code accesses TTY or OpenGL.
The original source may be resized/destroyed after successful capture.

`tango_grid_snapshot_test` exercises real C11 Unicode cells, independent
storage, forged public structs, clipping, resource budgets, failure
atomicity and lifetime across source resize/retirement on all three native
platforms. This proves only snapshot ownership; it is **not** an OpenGL
presenter, GL glyph atlas, GPU shader or device-retirement qualification.

## Native TTY presenter

Owns/borrows the terminal, validates the bounded buffer, produces VT diff
output and handles terminal input and resize on its UI owner. The terminal
is an **external resource**: it must outlive a borrowed renderer. No GL,
shader program, glyph atlas, or GPU fence is part of this path. Terminal
colors (16/256/truecolor) and cell attributes must be independently
admitted. Unsupported blur, shadow, per-pixel alpha and rotation must
not silently become a different appearance.

All existing real TTY/ConPTY/PTy behavior and CI requirements remain
mandatory. A headless `tui_buffer_t` renderer is a test/presenter
option **within the TTY/cell branch**, not an OpenGL implementation.

## Tango OpenGL presenter (implementation target, not current support)

The Tango owner still produces a cell grid. An **explicit, typed**
`TangoGridSnapshot` boundary must freeze cells, spans, grapheme bytes,
generation and row/column dimensions. The OpenGL presenter consumes it
without parsing terminal escape sequences or running another layout.
It uses the existing gCanvas/OpenGL infrastructure for context-owned
resources where its proven APIs fit, plus a dedicated terminal cell
rasterization pipeline:

- cell background quads + per-cell foreground/style instance data;
- glyph atlas pages for text/graphemes and explicit color-emoji policy;
- wide-cell continuation rendering, glyph metrics and atlas invalidation;
- cursor, decorations, selection and clipping with shared cell hit geometry;
- bounded dirty-rectangle uploads and framebuffer damage;
- explicit framebuffer/alpha/color-space/DPI/resize mapping.

The cell-to-pixel projection itself must declare its font metrics, scale,
integer grid dimensions, viewport and source revision. **This M2
effect-admission slice does not implement that projection.** A qualified
TangoOpenGL NativePixel effect must consume the projected texture/content;
a raw `EffectInput::Cells` is rejected rather than relabeled as an RGBA
surface.
`BackendApi::TangoOpenGL` admits only an explicitly selected **NativePixel
effect** applied after the projection, with a qualified desktop GLSL
artifact, GPU stage, installed-program observation, device limits and
native resource owner. It cannot admit a `TerminalCell` output surface,
silently convert arbitrary cell units to pixels, or borrow another
backend's artifact identity. Graphics (vertex+fragment) must be
implemented and qualified first; compute is never implied by OpenGL
availability and requires an independent execution/limits gate.

The new enum/profile **is not a claim that these assets, shaders, glyph
uploads, context recreation or GL entry points already exist**. No
Tango-OpenGL capability snapshot/manifest/factory is registered here.

## Ownership, failure and input

TTY owns terminal output completion, not a GPU fence. Tango OpenGL must
use the GL context/share-group owner and device generation; GL objects are
allocated and retired only with a current compatible context. A frame
must retain glyph pages, buffers and pipelines until its actual GPU
completion. Use the existing `NativeEffectResourceOwner` reservation
and verified scope, but a prepared effect is not permission to release
in-flight GL resources. Device/context loss invalidates only the
corresponding GPU plans; never reroute to TTY or render via a borrowed
stale context.

Physical key/PTY input belongs to TTY. GL-window keyboard, IME, pointer
and wheel input must be mapped through Tango's existing event model and
the canonical cell metrics on the UI owner. A pointer at a cell boundary
cannot hit a different Element than the one painted at that boundary.
Neither renderer may reparse RichDoc/Markdown or maintain a duplicate
business model.

Bound separately: cell count, grapheme payload, snapshot history, atlas
pages/bytes, shader variants, instance buffers, dirty uploads, frames
in-flight and retiring native objects. Check combined live + candidate +
in-flight + retiring peak cost before publishing the new GL plan.

## Acceptance gates (not completed by the enum change)

1. **Profile isolation:** explicit TTY remains shader-free and
   `TerminalCell`; TangoOpenGL requires `NativePixel` for GPU
   presentation/effects; the generic OpenGL profile cannot masquerade
   as TangoOpenGL; absent TangoOpenGL factory fails cleanly.
2. **Shared cell semantics:** C11 Tango unit tests plus positive and
   negative Unicode/grapheme, CJK-wide, combining, emoji, cursor,
   selection, clipping, resize and scrolling fixtures for *both*
   presenters. TTY real PTY/ConPTY remains mandatory.
3. **Real OpenGL:** context supplied by the actual desktop host, exact
   GL/profile and driver recorded, shaders compiled/linked with
   verified resource identity, screenshot/pixel and pointer-hit
   conformance. Software GL is labeled separately from physical GPU.
4. **Lifecycle:** failed GL compile/atlas allocation, stale generation,
   context loss, repeated resize, in-flight frame retirement, old/new
   plan peak budget and rollback all tested without fallback.
5. **Integration:** single XML/Tailwind/Theme/Binding/controller
   semantics, no CSS parser, no GPU work on the TTY path and no
   second Tango renderer VM.

Until all matching target gates pass, the GL presenter is only a
reserved public selection and a **synthetic CPU admission profile**.
The existing TTY implementation and its tests must not be deleted,
weakened or repurposed to pretend OpenGL support.

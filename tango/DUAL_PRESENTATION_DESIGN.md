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

## First GL pixel presenter implementation (experimental)

`flex/backends/tango_opengl/cell_presenter.cpp` now stages a typed
`CellPixelPlan` from the immutable C11 grid. It keeps every cell background,
leading grapheme and wide-cell continuation, supports explicit reverse/bold/
dim/underline/strike operations and rejects italic/blink/unknown styles.
It also bounds cell counts, draw commands, UTF-8 bytes and framebuffer
dimensions, and checks the selected font's measured glyph bounds before
beginning a pixel frame. This is a **one-glyph-per-leading-cell** first
profile, not the final instanced glyph pipeline; source Unicode remains Tango
C11, not a new TUI parser. The frame plan is immutable to callers.

`gl_presenter.cpp` creates a **real external gCanvas OpenGL renderer**
from an explicitly current host context and proc loader. It owns
gCanvas's existing GPU shader/glyph context, uses no alternate NanoVG
engine and does not register `RendererBackend::TangoOpenGL` until separate
GPU runtime, input, owner-retirement and product acceptance completes.
The demo `flexUI/examples/tango_opengl_demo.cpp` opens an actual GLFW
GL 4.1 context, captures real Tango cells, presents the staged frame and
swaps the host window buffer; an optional font path selects the exact
monospaced font. The demo is built only with explicit
`-DFLEX_BUILD_BACKEND_TANGO_OPENGL=ON`, gCanvas GL, Tango and desktop
examples enabled. The host must destroy presenter resources with the
compatible context current.

The ordinary headless cross-platform test `test_tango_gl_frame` uses a
recording pixel renderer to prove cell geometry, UTF-8/CJK span
projection, reverse/color/decoration, revision/budget/no-fallback and
the owner-thread contract. **Those CPU tests do not qualify real GL
pixels, font coverage, multi-frame GPU fences, resource retirement or
input hit testing.** The production public `TangoOpenGL` factory remains
unregistered and unsupported until #342's native GPU acceptance.
A driver rejection after frame begin is not an atomic GPU rollback.
This new source does not change native Tango TTY behavior.

## Mesa / Xvfb actual GPU pixel qualification

`tango/tests/opengl-smoke/` configures the **real** `tango` C11,
`Flex::Render`, gCanvas OpenGL shader/glyph renderer, and the
separate `flex_backend_tango_opengl` from their exact production sources.
It deliberately does not import the old XML/CSS UI app, Flex DSL parser,
NanoVG or a fake GPU renderer.

`tango_gl_real_pixel_smoke` creates a real GLFW OpenGL 4.1 core
context under Mesa/Xvfb, selects two explicit regular/bold monospaced font
files (both required, no implicit default family fallback), captures a
Tango-owned grid and renders it through the actual gCanvas GL graphics
pipeline. It reads back GL framebuffer pixels and validates independently
colored backgrounds plus the presence of a rasterized glyph; failure
before frame submission must leave the preceding GPU image intact.
It writes the actual `glReadPixels` frame to a PPM artifact. CI logs the
driver GL vendor/renderer/profile and immutable source HEAD.

The independent `Tango OpenGL native pixels` workflow uses pinned
SHA256-verified Salts SDK, pinned vcpkg baseline, separate dependency
cache, verified ccache and an explicit llvmpipe driver. It is not a CPU
mock and is not a physical GPU test. The exact-HEAD pixel evidence,
if/when passing, qualifies only this fixed 8×4 ASCII/colored-cell GL
scenario. It does **not** prove Unicode font coverage, cell→pixel hit
testing, window IME, device-loss/retirement or a bounded atlas-wide
pipeline. Keep the public `RendererBackend::TangoOpenGL` factory
unregistered, and #342 open, until those separate production gates pass.

## Pixel-hit geometry from the actual submitted cell frame

`CellPixelPlan` now freezes row/column extents, cell width/height and a
canonical leading-cell map alongside the exact GPU background/glyph commands.
`hit_test_logical()` and `hit_test_framebuffer()` use **those same**
top-left-origin drawn cell bounds, returning a `CellPixelHit` with both
the physical cell and the leading column for a wide grapheme. A click in a
CJK continuation cell retains its physical column but targets the leading
grapheme. Edges are left/top inclusive and right/bottom exclusive; NaN,
infinite, negative and outside coordinates are not hits.

`hit_test_window_pointer()` maps GLFW window logical coordinates through
the **observed, same-generation** window/framebuffer extents and the plan's
pixel ratio. Mismatched DPI/resize dimensions cause an explicit diagnostic,
not a guessed scale or silent fallback. No terminal-cell layout is recomputed.

`CellPixelPresenter::hit_test_window_pointer()` reads only the last
synchronously successful submitted frame on its UI/GL owner thread. The
presenter retains its owned plan rather than borrowing the mutable Tango
source. Invalid source revision and unqualified font metrics are rejected
**before** native drawing and preserve the old submitted hit plan. Failure
after native drawing begins makes the previous hit plan unavailable, because
partial external GPU output cannot be rolled back or trusted. The diagnostic
`last_submitted_plan()` is a borrowed pointer for synchronous inspection
only; do not retain it across another `present()`.

The existing real Mesa framebuffer test asserts that physically sampled
red/green/background pixels select precisely their expected cell from the
GLFW-reported window/framebuffer dimensions and that stale-grid rejection
keeps both the old GPU pixels and hit generation. Native CPU tests include
CJK wide continuation, 1x/2x DPI mapping, exclusive boundaries, malformed
coordinates, resize mismatch, owner-thread refusal and draw failures.

The GLFW desktop demo can report mapped left-button cell clicks for
diagnostics. **Still pending:** wiring actual window keyboard/IME/wheel and
mouse events into the canonical Tango event queue, a real injected X11
pointer/IME conformance, context recreation, device-generation invalidation
and GPU in-flight completion/retirement. Do not register production
`RendererBackend::TangoOpenGL` yet.

## GLFW native callbacks -> canonical Tango C11 input (Draft)

The OpenGL host can now convert real GLFW mouse-button/cursor/scroll,
navigation-key and committed Unicode character callbacks directly into
`tui_event_t`. `cell_input_pointer()` uses ONLY the last successfully
submitted `CellPixelPresenter` geometry; the returned sidecar carries the
frame revision and wide-grapheme leading column while native `event.x/y`
retain physical cell coordinates. The adapter is a synchronous UI/GL-owner
operation, **not** a new queue, parser, terminal model, or input thread.

Special-key callbacks and committed-character callbacks are disjoint (no
double delivery of printable text). Unsupported modifier/action combinations,
stale host framebuffer metrics, and wrong-thread calls fail explicitly.
Unicode input is accepted only as a valid committed scalar. GLFW does not
provide cross-platform IME preedit through `glfwSetCharCallback`; composition,
accessibility, key-up, keyboard layout/control chords, out-of-window capture,
mouse drag, live resize, injected OS event/focus acceptance and GPU context
recreation remain **open**. Real Mesa acceptance checks the event adapter
against actual framebuffer pixels; that is **not** an OS-injection test.
No production TangoOpenGL factory registration is authorized.

## Real X11/XTest injected input acceptance (Mesa/Xvfb)

The existing native Mesa/Xvfb test additionally requires the actual XTest
server extension and a mapped, focused GLFW X11 window. Server-injected
left-button press/release on the *real red and green GPU cells* and Left/A
keyboard events must arrive through GLFW callbacks and be normalized into
the canonical Tango C11 `tui_event_t`, preserving exact submitted frame
revision 101. The first injected click drives a new native C11 source snapshot
and **real GL framebuffer highlight** at revision 102, captured separately
as `tango-gl-frame.ppm.input.ppm` alongside the initial GPU frame.
No direct callback invocation, in-memory pixel substitution, synthetic
`tui_event_t` queue or fallback renderer is accepted.

This qualifies Xvfb/XTest input on the Linux software Mesa path only.
Actual physical hardware, Windows/macOS event injection, IME preedit,
mouse capture/drag, resize/HiDPI transitions, focus-loss retirement,
GL context loss/recreation and GPU fence ownership remain open. Keep
the public TangoOpenGL backend unregistered.

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

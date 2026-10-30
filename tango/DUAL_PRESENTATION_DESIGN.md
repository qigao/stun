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

## Focus / resize-generation input retirement (experimental)

`CellHostFrameGate` is a synchronous, owner-thread-only input **admission**
layer around the existing `CellPixelPresenter`. The last source revision is
not enough: two separate native draws can use the same C11 grid revision.
The presenter therefore records a monotonically increasing successful
`submission_id()` (zero if no completed frame is available). The host
explicitly calls `accept_presented_frame(observed window+framebuffer size)`
**after** native presentation and swap. Only that exact submitted id, source
revision, current focus and matching logical/device-pixel dimensions can
produce mouse, keyboard or committed-text `tui_event_t` values.

GLFW focus-out/focus-in and logical-window/framebuffer size callbacks retire
input. Neither a focus regain nor restoring old dimensions can reactivate a
previous GPU plan; re-admission requires a **new successful draw and swap**.
A failed native frame or silent metric change also retires input. Rejected
pre-submit source/font validation does *not* retire a still-valid prior
frame when focus and host geometry have remained coherent.

The existing Mesa/Xvfb test now uses actual server-side X11 focus changes
to the root window and back, with XTest keyboard injections while unfocused,
then requires fresh GPU presentation before real keyboard recovery. It also
uses an actual `XResizeWindow` event to prove that **unrepainted** resized
geometry cannot accept a stale C11 event. This is a **negative resize gate**
and does not implement successful live resize or DPI reflow. The same
strict admission is used by the GLFW demo, with no second queue/model.

**Still OPEN:** positive physical GPU resize and HiDPI adaptation,
nonuniform scaling policy, IME preedit and system text services, drag capture
outside window, accessibility, X11/Windows/macOS physical GUI acceptance,
GPU fence completion and GL context loss/recreation. No production factory
can be registered solely from these tests.

## Live positive Resize -> real GL frame -> new cell hit (Draft)

The experimental fixed-count C11 `tui_grid_snapshot_t` remains the **sole**
source of glyph/grapheme/span/attributes. `fit_cell_metrics_to_window()`
derives a new cell/pixel projection from GLFW's **current observed** logical
window dimensions and device framebuffer extents, and scales font size /
baseline coherently with cell height. The X/Y device ratios must agree.
The resulting float draw geometry must round exactly onto the observed
framebuffer; incoherent DPI generations, stale source revision, empty window,
oversized spans and unsupported glyph metrics fail before publication.
This is one GL renderer and one source grid, not a new layout or TTY-cell
pixel assumption.

The existing GLFW demo now enables native resize, retires input during
minimize/zero framebuffer, reprojects its unchanged Tango C11 source grid,
redraws through the existing gCanvas presenter, swaps, and admits mouse/key
events only for the new submission.

The existing **real X11/XTest + Mesa llvmpipe** pixel test requires an actual
two-axis `XResizeWindow` event from 256×96 to 304×120, verifies stale
keyboard/text are refused before repaint, renders real GPU yellow and green
cells at their NEW positions, reads the native resized framebuffer back to
the added `.resized.ppm` audit artifact, then sends an actual XTest mouse
click into the new green cell (5,1), demanding source revision 102.
This qualifies **real software OpenGL resize at observed 1× DPI**, not a
physical high-DPI monitor. Native C11/CPU tests additionally exercise 1×,
1.5× and 2× device pixel ratio projections with exact hit-test bounds, but
those synthetic device dimensions are **not** proof of real HiDPI rendering.

Still open: actual multiple-monitor live DPI changes/2× framebuffer on
supported devices, generic XML/Tailwind reflow when grid rows/columns change,
IME preedit, pointer capture/drag, physical GPU acceptance, platform-native
Windows/macOS event injection and GL context retirement. No production
TangoOpenGL registration is justified yet.

## Controlled native GL context teardown and fresh-context acceptance (Draft)

`CellHostFrameGate::retire_device()` now permanently disables one host
gate **on its original UI/GL owner**, clears its captured submission
identity, detaches its presenter pointer, and refuses input even if the gate
outlives the native GPU renderer. A new GL context must create a **new**
`CellPixelPresenter`, **new** `CellHostFrameGate`, successfully draw a frame,
and publish the actual host dimensions before accepting input. Old gates
cannot be reactivated by a refocus or a freshly created unrelated context.
Wrong-thread retirement is rejected, and same-owner retirement is idempotent.
The GLFW demo unregisters callbacks, then retires its gate while the GL
presenter/window are still alive. Do not let callbacks hold a pointer past
native GL presenter destruction.

The existing native Mesa/Xvfb pixel qualification additionally creates
**two sequential real GLFW OpenGL contexts** on the same X display, without
a shared context or second renderer implementation. It draws and reads
real native framebuffer pixels from the first context, drains the frame,
retires the old input gate, destroys the actual gCanvas GL presenter
while its own context is current, destroys that GLFW window/context,
then creates a new GLFW context/presenter/gate and draws a *different*
colored C11 frame at the **same source revision**. The replacement image
is captured as `tango-gl-frame.ppm.recreated.ppm`. The old gate must still
reject all events after the new context is created and after destruction,
whereas the new gate admits its own newly submitted cell geometry.

**Narrow scope:** this is an orderly/drained GL teardown and recreate,
not a proof of abrupt device/context loss, uncompleted in-flight GPU work,
async fence retirement, invalid GL resource deletion without a current
context, or GL recovery after a native driver reset. Those remain strict
OPEN gates along with physical GPU, true monitor HiDPI, IME and native
Windows/macOS event injection. No production TangoOpenGL factory registration.

## Native GLsync bounded frame completion and resource retirement (Draft)

`NativeGLFrameFences` is a **context-owned, owner-thread-only** GLsync
retirement contract layered over the *existing* gCanvas GL shader/cell renderer.
It resolves `glFenceSync`, `glClientWaitSync`, `glDeleteSync`, `glFlush` and
`glGetError` through the exact host-provided OpenGL proc loader.
There is no second GL backend, fallback renderer or synthetic GPU completion.
The host supplies a mandatory exact-current-context predicate and a nonzero
`device_generation`. A new GL context uses an independent fence owner
and new generation even if its C11 grid revision/submission counter repeats.

Before a new GL frame, `require_capacity()` enforces a small explicitly
bounded in-flight budget. After the native renderer successfully completes its
pixel submission, `record_submitted_frame(presenter)` inserts a real
`GL_SYNC_GPU_COMMANDS_COMPLETE` fence and flushes. It records the
source revision, GPU-owner generation and strictly increasing presenter
submission ID. Duplicated/invalid submissions and a full queue fail closed.

`retire_completed(timeout_ns)` calls the real `glClientWaitSync` with a
maximum of 100ms per poll; timeout returns Busy and keeps the GPU fence
owned. Only `GL_ALREADY_SIGNALED` or `GL_CONDITION_SATISFIED` permits
`glDeleteSync`, FIFO removal and publication of the retired submission.
Wrong-thread/wrong-context operations are refused **before calling GL**.
`close()` refuses while any fence remains. An abandoned object with
outstanding GLsyncs fails fast rather than deleting them against a dead
context. A bounded drain must complete **before** the gCanvas presenter and
its glyph/atlas resources are released.

The existing native Mesa/Xvfb test qualifies real fences against **two
sequential distinct GLFW/OpenGL contexts** and actual gCanvas-rendered C11
frames. It verifies queue capacity refusal, premature close denial,
wrong-current-context and wrong-owner denial, explicit bounded real GPU
signal, safe GLsync deletion and fresh-device independent fences at the
same source revision. Its old X11/XTest pixel/input/resize qualifications
remain mandatory.

**Scope:** This proves real GLsync retirement for **orderly completion on
Mesa llvmpipe**, not long-running GPU overlap, all gCanvas internal async
uploads, application-wide shader/atlas lease retention, hardware device loss,
fence failure after abrupt reset or context-free GL deletion. The current
GLFW example has not yet been migrated onto this explicit fence holder;
therefore production TangoOpenGL registration remains prohibited.
Unexpected context loss, device-generation quarantine and GPU resource
retirement during asynchronous shutdown still require independent gates.


## Combined native gCanvas GPU resource owner (Draft)

`NativeGLPresentationOwner` wraps **one actual gCanvas**
`CellPixelPresenter`, **one native `NativeGLFrameFences` queue
and the canonical `CellHostFrameGate`. It is compiled only
by the explicit TangoOpenGL backend. Headless/C11 modules remain free of any
GLAD/OpenGL SDK headers or libraries.

The owner exposes a **const-only** presenter view and retained (shared)
input-gate reference. The host cannot obtain a mutable presenter or GLsync
owner from this object and must call `present_and_fence()` to draw;
this checks bounded capacity against the ORIGINAL current GL context and
creates a real native GPU fence after the existing gCanvas renderer has
submitted its glyph/shader commands. A successful grid revision alone does
not qualify frame completion. On a possibly unfenced native drawing error
the owner enters a terminal unsafe state; no renderer deletion or fallback
is allowed. A definite pre-submit refusal with an intact prior submission
does not mark the device as broken.

`close()` is the only path that actually deletes
gCanvas's pixel renderer, shader and glyph resources. It first requires
every real GPU fence to be signaled/retired, then permanently retires the
input gate and deletes the presenter **while the original context remains
current and on its owner thread**. It refuses premature close, foreign
context, or a potentially unfenced device error. Retained input gates must
remain safe after the owner is closed, and may never revive on an unrelated
new context. Destruction with unsafe outstanding work fails fast; it does
not pretend to recover an abruptly lost GPU.

Existing Mesa/Xvfb dual-native-context tests use this COMBINED owner,
not independently managed native presenter and fences. The test requires
real GLsync capacity/owner/context refusal, rejection of early deletion
of gCanvas resources, bounded actual GPU completion and only then a
successful native close. The second new context again paints distinct
GPU colors at the same C11 revision while old input gates remain retired.

**Not yet qualified:** the standalone GLFW demo is still on the earlier
manual presenter API, so production registration remains forbidden.
gCanvas-internal *per-upload* atlas page lifetimes, fence timeouts under
real hardware overload, abrupt context loss and device-generation
quarantine/recovery remain separate strict gates. Do not infer them from
the software-Mesa two-context experiment.

## Strict serial glyph/atlas/image mutation barrier (Draft)

The existing gCanvas `NativeGLPresentationOwner` now enforces a **strict
single outstanding GPU submission per native renderer** while the backend
lacks independently proven font-atlas/image/shader subresource leases.
`require_capacity()` first checks the exact active native GL context,
owner thread and bounded GLsync queue. If even one previous submission's
native GLsync remains owned, it **rejects** any new `present_and_fence()`
before gCanvas measures glyphs, uploads atlas textures, draws geometry or
changes its shader/texture state. This is intentionally stricter than the
fence queue's configured `max_in_flight`. A capacity of two or
more does NOT authorize overlapping mutable resource use.

Only real native `glClientWaitSync` completion and
`glDeleteSync` retirement of the previous submission unlocks
the next draw. Neither an unobserved GPU signal, a window swap, a new C11
revision nor a presumed synchronized font cache counts as GPU resource
retirement. A time-out leaves the old frame owned and the next mutation
blocked. Current shared resource updates remain restricted to the same
owner/context as the presenter; no second mutable pixel renderer is exposed.

The existing Mesa/Xvfb dual-context test additionally creates a fresh native
gCanvas renderer per context with configured **2-slot GLsync capacity**.
It draws and fences a real red 'A' cell, attempts a second C11 frame
carrying a new ASCII 'Q' glyph and magenta background while one fence is
still owned, and requires a rejection with the original submitted geometry
and GPU submission counter unchanged **despite spare fence capacity**.
After observing real native fence completion, it submits the Q frame,
reads the actual magenta/green framebuffer and real white glyph, captures
`tango-gl-frame.ppm.recreated.ppm.serial.ppm`, and drains
the second real fence before closing the GPU owner. Both old and new
separate contexts must pass; original XTest/focus/resize/ownership evidence
continues in the same existing Mesa CI job.

**Important limit:** this contract proves conservative non-overlap for one
presenter, not that `glTexSubImage2D` actually ran for every new
glyph, nor that independently owned gCanvas images/shader pipelines have
per-resource completion scopes. It sacrifices overlapping in-flight frames
until actual atlas/image/shader leases, budget and resource retirement are
proven. Physical GPUs, unexpected device loss and cross-platform native
window/IME gates remain open. Keep the public TangoOpenGL factory disabled.

## Actual gCanvas native resource identities and fence lease ledger (Draft)

The real gCanvas GL context now exposes a **bounded, GL-backed resource
inventory** through a source-compatible optional `Context` API.
The implementation inspects its **actual** created Shader Program, the
internal Texture, uploaded Image Textures and Font/Glyph Atlas Textures.
It calls the original GL driver's `glIsProgram` and
`glIsTexture` on every included native name, rejects duplicate
texture identities and refuses a stale/wrong-current context. Resources not
uploaded to the GPU are not labeled live. The same GL implementation also
continues to serve gCanvas GLES; backends that do not implement the optional
inventory return unsupported instead of fabricated IDs.

The existing Flex/gCanvas renderer maps the actual GL resource IDs into
SDK-free `NativeRendererResourceId` values. The same TangoGL
`CellPixelPresenter` requires a successfully submitted frame
and UI-owner thread before returning the inventory. There is no second GL
renderer, global resource registry, shadow atlas or guessed resource plan.

`NativeGLPresentationOwner::present_and_fence()` captures
these real shader/texture/atlas IDs *after* drawing and *before* inserting
the real GPU GLsync. Its bounded `NativeGLResourceLease`
records combine kind, native name, exact native device generation, canonical
C11 source revision and presenter submission ID. Every uploaded resource
is conservatively considered potentially used by that frame; this is an
over-approximation, not a claim of per-glyph draw usage. An actual
`glClientWaitSync` completion/retirement marks all attached
leases complete. No next draw or whole-presenter destruction can proceed
while any resource lease remains unretired. The already qualified strict
serial barrier remains in force; **parallel frame admission is not enabled**.

The existing Mesa native GPU test now verifies positive Shader/Atlas/Internal
Texture resource identities under both independent GL contexts. It checks
that the IDs are real nonzero GL names from the actual gCanvas context, carry
the exact device/submission/source revision and remain pending before
native GPU completion. After real fence retirement they become completed,
then the second C11 glyph/color frame leases the **same native shader and
atlas names** under a *new* submission ID. Both sets of resource leases
must retire before any renderer/context is destroyed.

**Important limits:** GL object IDs are unique only within a live context
(and resource namespace) and must never be used without their generation.
The lease snapshot is **conservative for the whole GL context**; it does
not distinguish different atlas pages' actual draw dependencies, prove
individual `glTexSubImage2D` calls, or permit overlapping
updates. Resource bytes/peak budgets, async shader/texture eviction,
unexpected driver loss and host callback retirement remain separately OPEN.
The standalone GLFW demo is still not migrated onto the combined owner,
so no production TangoOpenGL factory may be registered.

## Native GL Texture/Shader mutation admission (Draft)

The existing gCanvas OpenGL Context now accepts an **optional native
mutation-admission callback** through its original OpenGL host configuration.
There is no second GL renderer, no global GL dispatch replacement, no
extra parser, and no probe/consumer workflow. Without that callback,
normal gCanvas GLES, Android and other consumers retain their existing
behavior. Tango's `NativeGLPresentationOwner` installs its
own callback while constructing the *original* gCanvas Context, so even
font/shader uploads during renderer creation require the real host's
same-current-GL-context identity and zero unretired native GLsyncs.

gCanvas calls this observer **immediately before actual driver operations**:
`glTexImage2D` (including dummy textures and font atlases),
`glTexSubImage2D` for the existing `Image::update_data()`
path, `glDeleteTextures` from the original native Image
destructor, and `glCreateProgram/glDeleteProgram` from the
existing GL Context. Upload/update admission refusal throws before
altering texture storage; destructor refusal terminates before deleting
a texture or program still referenced by the GPU (there is **no**
context-less or CPU-only fallback).

The combined native owner additionally has an explicit **cumulative CPU
texture-payload upload byte budget** (default 1 GiB, configurable up
to 2 GiB). It is checked before the native `glGenTextures/glTexImage2D`
pair and is deliberately conservative: this is **not** GPU VRAM
telemetry, driver allocation overhead, mipmap storage, resident image
peak, uniform/shader memory or an overall device memory budget. A tighter
budget can refuse atlas creation before GL texture storage is allocated.
The audit reports accepted native upload/update/delete and program
create/delete attempts, denied calls and cumulative payload bytes.

The original Mesa/Xvfb test now also uses the real low-level gCanvas
OpenGL Context and an independently created **actual native GLsync**:
a texture with real red GPU pixels receives a denied
`Image::update_data()` call while its earlier GPU fence is
not acknowledged; `glGetTexImage` must still return red.
After actual `glClientWaitSync` signal and `glDeleteSync`,
the same resource updates via real `glTexSubImage2D` and GPU
readback must contain green texels. Context cleanup must report
`glDeleteTextures` and `glDeleteProgram` callbacks.
The combined TangoGL owner also audits its real font/shader uploads and
deletes across two independent Context generations and rejects a
tiny upload-byte budget before creating a large font atlas.

**Remaining strict limits:** actual total device VRAM/peak GPU memory,
native per-resource eviction or concurrency and per-atlas-page leases,
driver reset/unexpected context loss recovery, physical GPU,
native IME and OS window callbacks on Windows/macOS. The previously
accepted strict serial-in-flight frame gate remains mandatory. Do
not register the production TangoOpenGL factory or treat this as a
physical-GPU driver recovery proof.

## Transactional CPU image update on native GPU rejection (Draft)

The earlier mutation guard prevented an unsafe `glTexSubImage2D` call but
`Image::update_data()` had already overwritten its retained CPU pixels
before the GPU backend was consulted. A rejected GPU update therefore
left CPU and GPU images disagreeing, even though the GPU frame had not
changed.

The same original gCanvas `Image::update_data()` now stages a bounded
complete candidate byte vector first. It publishes that candidate only
for the existing owning Context's synchronous upload, then **rolls back
both retained CPU pixels and its data pointer** if the Context rejects
the update or throws. Both swaps are non-throwing; malformed input never
touches the original image. Data passed from the current retained image
buffer is handled without invalidating its source during staging.

The existing Mesa native texture test is expanded to inspect **both**
the actual GPU `glGetTexImage` texels and the retained
`Image::pixels()` bytes after a denied in-flight update, verify normal
CPU+GPU green commit after the real native GLsync retires, then reject
a malformed source span while preserving the last committed green CPU
bytes. It also exercises source-data aliasing with
`Image::pixels().data()`, not another renderer or fake texture.

**Narrow guarantee:** this proves strong CPU rollback on a backend
exception and the particular **pre-driver-write** native Fence rejection.
It is **not** GPU transactional rollback after a driver error or partial
`glTexSubImage2D` operation, and is not a replacement for unexpected
Context Loss quarantine or physical GPU in-flight retirement. The
existing native Context/GLsync gate, serialized Atlas mutations and
bounded upload-budget rules remain unchanged. The production TangoGL
factory stays unregistered.

## gCanvas transactional image-update modeled peak admission (Draft)

The same gCanvas `Image::update_data()` now performs an **early peak
payload preflight** before allocating the extra CPU candidate vector.
The owning `gcanvas::Context` counts the exact retained **CPU pixel payload**
across its image objects and font atlases, plus the prospective candidate's
entire source payload. Existing desktop OpenGL and OpenGLES backends also
report the **actually uploaded base-level texture payload** from
`ImageImplGl`, including dummy/internal, image, and font-atlas textures.

`ImageUpdatePeakUsage` reports:
`retained_cpu_bytes`, `candidate_cpu_bytes`,
`native_uploaded_base_bytes`, `modeled_peak_bytes`,
`limit_bytes`, and `native_payload_known`. Non-GL backends never
fabricate native GPU byte counts: `native_payload_known=false` explicitly
marks CPU-only accounting. Overflow and an over-limit candidate fail
**before candidate allocation**, leaving both the existing CPU Image
and GL resources untouched. `ResourceLimits` defaults to a **2 GiB
image-update modeled peak cap**, with an explicit limit up to 4 GiB.
The original Context can lower/restore its next-update admission cap
without evicting existing images; this does not silently allocate
a second renderer/texture or reset its native GL ownership.

An in-flight GLsync referencing an uploaded texture does NOT allocate
another copy of that texture: its base pixel payload is counted exactly
once in `native_uploaded_base_bytes`, including during in-flight
GPU reads. The existing strict one-in-flight GPU fence/Atlas mutation
gate continues to protect the same GL storage.

Existing Xvfb/Mesa native context acceptance now measures the actual
CPU/GL payload model, reduces the live Context limit by **one byte**
below the measured transaction peak, and requires that an attempted
image update is rejected *before GPU callbacks or CPU candidate
mutation*. Both the retained CPU green pixels and the actual
`glGetTexImage` green pixels must remain intact. Restoring the
original cap requalifies the same image payload. This piggybacks
on the existing real texture and native GLsync test; no new
consumer, CI workflow, or artificial GPU implementation.

**Strict interpretation:** this is an admission bound on *modeled image
update transaction payload*, not total CPU RSS, vector capacity,
intermediate FreeType staging memory, mipmap allocations, GPU residency
fragmentation, driver VRAM, shader buffers, or application-wide peak
resources. Construction of Fonts/Images remains subject to its separate
existing budgets; the new cap does not retroactively evict live resources.
Crashed/partially written GL operations still require device quarantine.
Physical GPU/HiDPI and production TangoOpenGL factory remain open gates.

## Native gCanvas Image/Font creation-time modeled peak admission (Draft)

The earlier image-update transaction budget does not stop the original
gCanvas **Image constructor** from copying a new pixel buffer, nor
FreeType from allocating an entire **Font Atlas staging buffer** before
the GPU upload gate. Those are distinct allocation risks from updating
an already-resident image.

This slice adds `ResourceLimits::max_image_creation_modeled_peak_bytes`,
an **independent 2 GiB default** (up to an explicit 4 GiB limit).
The owning original `gcanvas::Context` computes
`ImageCreationPeakUsage`: all retained CPU Image + Font Atlas pixels
and internal GL dummy pixels, current uploaded **native base-level**
GL/GLES texture bytes, plus the prospective persistent CPU pixels,
all simultaneously live transient CPU staging buffers, and future
native base texture pixels. Every addition is overflow-checked and
the result must fit before allocating the new CPU buffer.

For raw-memory Images, the existing `ContextImplGl::create_image()`
checks dimensions and the exact borrowed byte span, then admits
persistent CPU pixels + eventual native base texture storage before
constructing the original `ImageImplGl`. For file-backed Images,
STB image metadata is inspected before full decode; the model
also counts STB's temporary forced-RGBA decode buffer alongside
the persistent CPU copy and native base texture. A file that
changes dimensions between inspection and decode is rejected
rather than accepted against a stale preflight; file-content
changes at identical dimensions remain outside this admission.

For Fonts, the existing `Font::load_from_file()` admits its file
buffer before allocation. The original `Font::load_from_memory()`
knows the FreeType Atlas dimensions before it allocates its large
temporary raster buffer and calls the same Context's peak admission
through the sole native `FontImplGl` owner. This counts the file
buffer when present, the **temporary raster pixels**, the new
persistent Atlas Image copy and the future native texture base
pixels together. The atlas source memory supplied by an external
caller, FreeType private allocations and per-glyph map nodes
are not included; they must be audited separately.

The existing Mesa/Xvfb native Context test lowers the image creation
budget to **one byte less than the measured real transaction**:
a raw-memory Image creation must throw `length_error` before
Image/GL allocation and preserve existing CPU and GPU texels.
A tiny one-byte budget must separately reject font-file allocation
and a real FreeType in-memory Font Atlas before any GPU upload.
After restoring the cap, a new native GPU Image must be created
and included in the original gCanvas uploaded-resource inventory.
The same test retains native GLsync, XTest, Resize and shader/atlas
resource retirement qualification.

**Strict scope:** a *modeled pixel-payload admission*, not whole-process
RSS or GPU VRAM. FreeType private buffers, font glyph maps, STB decoder
bookkeeping, vector capacities, mipmaps, texture padding, fragment
shader/uniform buffers, driver allocations, concurrent owners and
pending cross-Context resources remain outside its accounting.
A font-file staging check and atlas check are separate proofs.
The existing GPU serial Fence/Lease, CPU rollback and original
Context ownership requirements are unchanged. No production
TangoOpenGL factory registration or additional CI/consumer.

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

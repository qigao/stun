# Real native GUI E2E — M0 X11 screenshot/interaction harness

This is a **real-window screenshot oracle**, not an HTTP service, mocked
renderer, in-process UI test or remote OCR implementation. Tracking:
[GUI E2E Issue #330](https://github.com/qigao/stun/issues/330).
Python code: .github/scripts/gui_native_e2e.py.

## Actual application target

Build the existing flexui_desktop_editor target from the normal Stun
CMake tree with real gCanvas OpenGL window, TurboScript and PluginHost.
The launcher starts that **built native executable with no flags**
and waits for a visible X11 window titled "FlexUI Desktop Editor".
The existing --smoke mode has an invisible window and is **not**
valid screenshot/visual E2E evidence.

The C++ example defines a 928x608 client window. Its deployed CSS gives
Save a blue background (#2563eb, hover #3b82f6). TurboScript changes
text from Save to Saving... and ultimately the document-service result.
The initial Save label is centered. The long service result stays on one
line with an ellipsis, so completion text cannot cover the document.

## Run the real native executable (Linux)

Prerequisites: working real OpenGL/GLFW build, xvfb-run, XTest,
python3-xlib, Pillow and tesseract-ocr. Product qualification requires
local OCR of the initial Save and completed Saved states. The screenshot
driver uses no Baidu OCR, network, secrets or HTTP requests.

~~~sh
# Install only if not already present:
sudo apt-get install -y xvfb xauth python3-xlib python3-pil tesseract-ocr

# Real built desktop editor, never hidden --smoke:
export LIBGL_ALWAYS_SOFTWARE=1
export GALLIUM_DRIVER=llvmpipe
xvfb-run -a -s '-screen 0 1280x900x24 -nolisten tcp' \
  python3 .github/scripts/gui_native_e2e.py \
    --exe build/bin/flexui_desktop_editor \
    --source-sha "$(git rev-parse HEAD)" \
    --out build/gui-e2e \
    --ocr-initial Save --ocr-after Saved
~~~

This refuses missing binaries, no X11 display, missing or ambiguous
visible windows, wrong dimensions, blank frames, absent/misplaced blue
Save button, missing native Save-click visual change or completed Saved
text, nonzero exit, or failure to close via WM_DELETE_WINDOW. First-frame
rendering and native completion use bounded observation of the real
screen; merely finding an X11 window or displaying Saving is insufficient.

Evidence files:

- before.png, hover.png, after.png: real X11 root framebuffer cropped to
  the visible client window, not generated from an internal widget tree
- *_ocr.png: derived crops inside the detected button (2px inset), with
  grayscale contrast normalization, inversion, 3x bicubic scaling and a
  white border. Every label uses the same preprocessing, preserving
  antialiased text strokes; the full-window frames remain unmodified.
- application.log: actual native process stderr/stdout
- evidence.json: exact source HEAD, native binary SHA256, PID, window ID,
  client rectangle, mouse action, changed pixel count, result/failure
  and OCR observations. Save layout evidence includes expected/observed
  bounds, each edge's delta and the allowed tolerance. A failed interaction
  retains the last actual post-click frame and OCR error for diagnosis.

The critical screenshot comparison is **hover vs. after real mouse click**,
not initial vs. hovered: a hover color is not proof of a click handler.
The button coordinates come from actual rendered blue pixels, never
from an unverified fixed coordinate. The native GUI workflow requires
both changed pixels and the whole OCR word Saved. Saving, Unsaved and
service error text cannot satisfy that completion requirement. An
uncertain OCR result is a failure, not an automatic pass.

### Independent Save geometry oracle

For the fixed 928x608 Linux editor, the required Save outer bounds in
client coordinates are `(720, 24, 856, 60)`: 24px shell padding, a 680px
title, a 16px gap and a 136x36 button. The actual blue-pixel bounds must
match all four edges within 4px, allowing border antialiasing and pixel
rounding. Discovering a blue control somewhere in the search region is
insufficient.

These expected bounds are an independent test contract; they are never
read from the runtime assets being qualified. After recognizing Save,
the driver waits within the initial-frame deadline for matching geometry
and reports `layout_mismatch` if it does not appear. It sends no Save
click to an incorrectly placed control. Click coordinates still come
from the detected pixels once the geometry qualifies.

This is one control's layout contract at the fixed test viewport, not
a full-screen golden or qualification of other DPI/viewport settings.

## Evidence boundaries

- test_gui_native_e2e.py tests color, placement and frame-change logic
  with deterministic frames **without a GUI**. These run in the
  existing Public Dependency parser job without new runner.
  A green unit test is NOT a successful native GUI E2E.
- The same test file also runs native X11 coordinate/capture/mouse
  regressions when STUN_GUI_X11_TESTS=1. These create an off-origin X11
  window and exercise the real python-xlib API. They verify the driver,
  and do not replace the subsequent Stun application run.
- A local Xvfb/Tk window validated the X11 screenshot/XTest transport:
  a responding test button passed, while an inert one failed.
  This is a harness self-test only, not a Stun renderer result.
- The real flexui_desktop_editor must still be built and executed on
  a GUI-capable CI runner to qualify #330 M0. There is no mock-window
  fallback, hidden --smoke substitution, or remote-OCR-only pass.
- Pin DPI, fonts and driver when introducing native visual goldens;
  do not assume identical pixels across Linux/macOS/Windows.
  Retain actual screenshots and diffs as bounded-lifetime CI artifacts.
- Keyboard focus traversal/activation and visible-window resize remain
  separate follow-up scenarios; native key routing or hidden-window tests
  alone do not qualify those real GUI interactions.

## Real application negative cases

The CI entry point is `.github/scripts/gui_native_suite.py`. It runs the
successful application first, then three isolated asset variants, sequentially
on the same display and runner:

| Scenario | Isolated asset change | Required visible failure |
| --- | --- | --- |
| Initial label | Change the Save button label to `Broken` in copied XML | A rendered blue button with OCR `Broken`; the driver rejects the initial state before clicking |
| Pending completion | Change the successful completion's displayed text to `Saving...` in copied TurboScript | Initial `Save`, a real click and changed pixels, then OCR `Saving`; the driver rejects the missing `Saved` completion |
| Misplaced control | Change the copied title width from 680px to 600px | Actual `Save` text and the same button size, but a detected position 80px too far left; the driver rejects the layout before clicking |

All variants execute copies of the same native executable and document
service plugin. Their SHA256 identities must match the successful baseline.
Each mutation must have exactly one source anchor and change only the named
asset; the original application and assets must retain their hashes. The
pending variant preserves the real command, plugin and completion callback.

~~~sh
xvfb-run -a -s '-screen 0 1280x900x24 -nolisten tcp' \
  python3 .github/scripts/gui_native_suite.py \
    --exe build/bin/flexui_desktop_editor \
    --source-sha "$(git rev-parse HEAD)" \
    --out build/gui-e2e
~~~

A nonzero exit alone is insufficient. A negative case must have the expected
visual failure code, a live application at the failure point, the real
window, the expected erroneous text in screenshot OCR, and a normal native
window-close exit. Missing dependencies, a blank image, empty OCR, a crashed
process or forced cleanup fail the suite.

The positive evidence remains at the output root. Each negative case keeps
its own screenshots, application log and `evidence.json` with `result: FAIL`.
The separate `suite.json` records whether those required failures were
actually observed. It reports PASS only after the positive and all three real
negative cases qualify; it never rewrites failed driver evidence as success.

## M1 native GUI build / actual-screen CI

The path-scoped workflow .github/workflows/gui-native-e2e.yml now targets
the real production flexui_desktop_editor through the minimal
.github/scripts/gui_runtime CMake composition (same repository-owned
production sources). It downloads SHA256-verified published native SDK
archives via curl, restores a pinned vcpkg compiler/dependency baseline,
uses verified ccache, builds the real OpenGL/GLFW GUI, then runs the
Python screenshot/XTest/OCR suite above under Mesa llvmpipe/Xvfb.
The restored vcpkg cache is exported in the installing shell. Completed
dependency builds and compiler results are retained even when a later
GUI qualification fails, without changing that failure into success.

The exact job name is "GUI visible screenshot / Linux X11", selected
by the trusted exact-head acceptance matrix for changes to the desktop
editor, the script or GUI build workflow. A CI success requires that
the actual window opened, rendered blue Save, reacted to mouse input,
displayed the Saved completion and closed correctly, and that all three wrong
visible states were rejected for their expected reason. Native source/UI
tests are not replaced.

The workflow is intentionally one Linux GPU-software window runner,
not a Windows/macOS matrix and not a new HTTP provider. Initial rollout
is strict: if the installed TurboScript/gCanvas dependencies or real
GUI are unavailable, the job must fail and remain Draft rather than
substitute hidden --smoke, fake Tk pixels or static XML assertions.
Screenshot artifacts are retained for 3 days; negative cases must
retain the failure reason even when the app failed to open a window.

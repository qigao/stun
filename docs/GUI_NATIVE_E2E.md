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

## Run the real native executable (Linux)

Prerequisites: working real OpenGL/GLFW build, xvfb-run, XTest,
python3-xlib and Pillow. Optional local OCR uses tesseract-ocr; core
tests use no Baidu OCR, network, secrets or HTTP requests.

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
    --out build/gui-e2e
~~~

This refuses missing binaries, no X11 display, missing or ambiguous
visible windows, wrong dimensions, blank frames, absent/misplaced blue
Save button, missing native Save-click visual change, nonzero exit,
or failure to close via WM_DELETE_WINDOW.

Evidence files:

- before.png, hover.png, after.png: real X11 root framebuffer cropped to
  the visible client window, not generated from an internal widget tree
- application.log: actual native process stderr/stdout
- evidence.json: exact source HEAD, native binary SHA256, PID, window ID,
  client rectangle, mouse action, changed pixel count, result/failure
  and optional OCR observations.

The critical screenshot comparison is **hover vs. after real mouse click**,
not initial vs. hovered: a hover color is not proof of a click handler.
The button coordinates come from actual rendered blue pixels, never
from an unverified fixed coordinate. OCR is optional and supplements the
pixel check. Use --ocr-initial Save --ocr-after Saved when enabled.
An uncertain OCR result is a failure, not an automatic pass.

## Evidence boundaries

- test_gui_native_e2e.py tests color, placement and frame-change logic
  with deterministic frames **without a GUI**. These run in the
  existing Public Dependency parser job without new runner.
  A green unit test is NOT a successful native GUI E2E.
- A local Xvfb/Tk window validated the X11 screenshot/XTest transport:
  a responding test button passed, while an inert one failed.
  This is a harness self-test only, not a Stun renderer result.
- The real flexui_desktop_editor must still be built and executed on
  a GUI-capable CI runner to qualify #330 M0. There is no mock-window
  fallback, hidden --smoke substitution, or remote-OCR-only pass.
- Pin DPI, fonts and driver when introducing native visual goldens;
  do not assume identical pixels across Linux/macOS/Windows.
  Retain actual screenshots and diffs as bounded-lifetime CI artifacts.

Next: integrate the **real Stun binary** with this script in a GPU-enabled
build workflow, without adding a GitHub App, Ruleset or redundant CI matrix.

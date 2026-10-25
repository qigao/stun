# Tango + FlexUI Streaming Chat Demo

This native demonstration uses the existing Tango C11 event loop, bounded asynchronous
mailbox, C11 `tui_transcript` registry, canonical Stun C11 Markdown parser,
FlexUI `MarkdownWidget`, and fixed-slot `TranscriptWindow` TUI adapter.

The producer is a deterministic scripted worker. **This is not a connected LLM
or TurboAgent product demo**, and it does not implement another renderer,
message queue, Markdown grammar, or actor scheduler.

## Build and launch

In an already configured Stun tree with the FlexUI and Tango TUI backend
enabled, build the optional example target:

```sh
cmake --build <build-directory> --target flexui_tui_streaming_demo
<path-to-built-executable>/flexui_tui_streaming_demo
```

The ordinary example is offered from `flexUI/examples/CMakeLists.txt` when
`NANOGUI_BUILD_EXAMPLES=ON` and `flex_backend_tui` is built. The existing
examples directory also configures desktop examples and may require their
GLFW/OpenGL build prerequisites; this sample itself has no GPU or window
dependency.

The focused installed-SDK headless project offers the same executable with
`STUN_BUILD_TUI_STREAMING_DEMO=ON` (the default for that project). Its
`--headless-smoke` option is enabled only in that CI-oriented build:

```sh
cmake --build <headless-build-directory> --target flexui_tui_streaming_demo
ctest --test-dir <headless-build-directory> --output-on-failure \
  -R tui_streaming_demo_smoke
```

That smoke uses a synthetic terminal output sink, but runs the real
`tui_app_dispatch_once()` callbacks and asserts the front cell buffer contains
rendered text after dozens of streamed messages and evictions. It does **not**
establish physical console/IME behavior.

## Controls

| Input | Action |
|---|---|
| Up / Page Up / mouse wheel up | Browse one older retained message |
| Down / Page Down / mouse wheel down | Browse one newer retained message |
| Home | View the oldest retained message |
| End | Follow the newest messages |
| c | Cancel the current scripted response |
| q, Esc, Ctrl+C | Stop the owner loop; quiesce producer; clean up |

The demo admits strictly increasing session-local message IDs, retains at
most 12 message states, and shows at most five Box-owned MarkdownWidget
hosts. Each admitted message and chunk obeys the existing copy-before-post
bounded mailbox contract. Once the producer has quiesced, the owner destroys
the window and C11 transcript before releasing the terminal.

## Acceptance boundary

- Real Tango OS wake and UI-owner event loop, terminal input polling,
  FlexUI layout/paint and terminal differential rendering are used by the
  interactive path. The renderer obtains the **current** terminal back buffer
  at frame begin, so it can be constructed before native terminal
  initialization and survive buffer replacement on resize.
- Only finished or cancelled C11 streams can be evicted at capacity.
  Evicted IDs cannot be reopened within a session. The fixed UI window
  reuses Element hosts and invalidates old generational handles.
- When the user navigates away from the tail, the application keeps a
  message-ID anchor rather than forcibly switching to the newest window.
- Tests exercise the same callbacks in a synthetic output sink on Linux,
  macOS and Windows. They do not claim full physical console input, IME,
  accessibility, arbitrary-height offscreen spacers, tool/diff folding,
  or complete Agent TUI readiness.
- `MarkdownWidget` still reparses accumulated Markdown on a published
  owner update; long-session CPU and memory benchmarks remain a separate
  qualification item.

Tracking: [Stun Issue #266](https://github.com/qigao/stun/issues/266),
[Draft PR #269](https://github.com/qigao/stun/pull/269).

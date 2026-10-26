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

### Experimental virtual-spacer terminal path (explicit opt-in)

Run the **same physical-terminal** demo with an application-local flag:

```sh
<path-to-built-executable>/flexui_tui_streaming_demo --experimental-virtual-spacers
```

The default invocation remains unchanged. This experimental path enables
the existing private `TranscriptHeightIndex` and **two Box-owned virtual
Spacer nodes** ahead of and behind at most five actual MarkdownWidget hosts.
The native C11 transcript still admits at most 12 monotonically increasing
message IDs, and the existing Tango UI Owner processes input and mailbox
events; there is no second queue, parser, event loop, or public Tango ABI.

- Heights of visible retained messages are recorded from **real Box layout**;
  unseen offscreen heights are explicit estimates, not pixel-exact claims.
- When resizing a physical terminal, the app updates Box viewport geometry
  **before** the height index checks width-based sample validity. A native
  `SIGWINCH` or ConPTY host resize cannot qualify against the old width.
- Home/End and cancellation keep their existing message-level semantics.
  In the **experimental** mode only, real Up/Down/Page and wheel inputs
  instead add or subtract three terminal-cell rows in Box layout coordinates.
  The existing UI Owner batches these deltas, maps the absolute viewport
  coordinate through the bounded retained-message height index, rebinds
  at most five hosts, then restores the message-local offset after real
  Box measurement. Two opposite moves within one Owner turn cancel without
  switching the user's original tail/history intent.
- Estimated offscreen heights are explicitly approximate. This is a
  private, fixed-step coordinate-to-window experiment, **not** a general
  touchpad precision-delta or scrollbar drag API, a character-accurate
  unseen-message locator, nor a complete production variable-height
  transcript virtualization implementation.
- The flag does not persist or change any user/default setting. It is intended
  for qualification and can be discarded without affecting normal users.

Independent CI test entries exercise the **unmodified owner/UI loop** under
kernel POSIX PTY with `--virtual-spacers` and Windows ConPTY with
`--virtual-spacers`. They preserve the ordinary PTY/ConPTY tests, external
visible cell assertions, 80×24 → 52×16 host resize and complete terminal
restoration. The POSIX fixture additionally checks a bounded out-of-band
post-Box owner frame for the expected host count and exactly two Spacers.
The experimental POSIX PTY also sends 24 real SGR wheel-up events after a
settled 12-message history and requires the first projected retained message
ID to move backwards, rather than accepting a `tail`→`scrollback` label alone.
An independent headless owner smoke checks the coordinate crossing and the
balanced Up/Down coalescing contract. A green experimental test does not
permit enabling virtualization by default.

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
| Up / Page Up / mouse wheel up | Default: browse one older retained message; experiment: scroll three terminal text rows towards history |
| Down / Page Down / mouse wheel down | Default: browse one newer retained message; experiment: scroll three terminal text rows towards newer content |
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
- The synthetic smoke exercises the real owner callback and cell buffer
  without an OS terminal; it is not a substitute for physical input or
  terminal restoration tests.
- Separate OS-driven tests qualify real POSIX PTY sessions on Linux/macOS,
  including Home/End, scrolling, SIGWINCH and termios restoration. Windows
  uses actual Console INPUT_RECORDs and a host-created ConPTY: the host calls
  ResizePseudoConsole(80x24 to 52x16) and checks displayed cells, 48
  completed messages, fixed header/footer and alternate-screen restoration.
  Five additional uninstrumented ConPTY sessions must all pass.
- The ConPTY test's VT screen oracle follows stream-ordered CSI 8;rows;cols t
  resize notices; a bare LF preserves the cursor column unless ANSI LNM is
  enabled. These are distinct from the process's Win32 Console state and the
  UI owner's private cell buffer.
- `MarkdownWidget` still reparses accumulated Markdown on a published
  owner update. No CPU/latency budget, Win32 IME composition, accessibility,
  arbitrary-height durable scroll spacers, tool/diff UI or downstream Agent
  product integration is claimed.

## Qualification evidence (2026-10-26)

- [Cross-platform #85](https://github.com/qigao/stun/actions/runs/40256909570):
  exact-head Windows 6/6 CTest plus five uninstrumented ConPTY runs, and
  real macOS PTY; Windows DECAWM cleanup retained terminal mode behavior.
- [Linux Runtime #860](https://github.com/qigao/stun/actions/runs/40257970209):
  installed-SDK ASan/UBSan 10/10 and a separate LeakSanitizer build 10/10,
  with `LeakSanitizer: ON` and `liblsan.so.0` observed in the log.
  This qualifies the headless XML/Streaming/PTY test paths only; it does not
  establish leak freedom of untested production Agent interactions.
- [Cross-platform #86](https://github.com/qigao/stun/actions/runs/40257970179):
  Windows and macOS headless/real-terminal tests succeeded on the LSan-gate
  integration commit. Production TurboAgent usage remains outside scope.

PRs #265 (Tango queue) and #268 (Markdown consumer) must be reviewed and
integrated independently before #269 is rebased/retargeted to master.
CI success alone is not an independent merge approval.

Tracking: [Stun Issue #266](https://github.com/qigao/stun/issues/266),
[Draft PR #269](https://github.com/qigao/stun/pull/269).

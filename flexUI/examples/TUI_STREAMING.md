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

### Bounded virtual scrolling (enabled by default in this Demo)

The ordinary physical-terminal launch now uses the retained-height index,
five recyclable Markdown hosts and two Box-owned virtual Spacer nodes:

```sh
<path-to-built-executable>/flexui_tui_streaming_demo
```

To compare with the historical message-by-message scroll behavior:

```sh
<path-to-built-executable>/flexui_tui_streaming_demo --legacy-message-scroll
```

The previous `--experimental-virtual-spacers` CLI flag is still accepted
as a compatibility alias for the **new default**. It is no longer necessary
to enable the virtual path. This only changes **this Streaming Demo's default
presentation policy**: other applications using the `TranscriptWindow`
API still choose whether to call `enable_height_tracking()` and
`enable_virtual_spacers()` explicitly. Neither Tango's C11 API,
the event loop, the queue, nor the application's retention limit changes.

- The native `tui_transcript` keeps at most 12 monotonically increasing
  message IDs, while FlexUI materializes at most five message hosts.
  Unseen history takes numeric heights in the two Spacer elements.
- Measured height comes from actual `MarkdownWidget` and `Box`
  layout. An unseen, never-rendered message has an **approximate** height.
  Scrolling into it refreshes the bounded window, measures it and preserves
  the retained message-ID + within-message offset anchor.
- Home/End preserve their oldest/follow-tail semantics; Up/Down/Page and
  mouse wheel move three terminal-cell rows through the height index.
  Opposing input in the same Owner turn cancels without changing the
  original follow/history intent. Native `SIGWINCH` or ConPTY resizing
  invalidates width-sensitive measured samples before rebinding.
- The explicit legacy mode skips the virtual index and Spacers, using the
  old one-retained-message-per-navigation-step behavior. It is a reversible
  diagnostic/user preference, not a second event loop or renderer.
- This is **bounded message-window virtualization**, not unbounded
  persistent history, touchpad precision deltas, pixel-perfect geometry
  for unseen variable-height GFM, or a finished TurboAgent/TUI product.

Independent installed-SDK tests must validate the *default* real POSIX PTY,
Windows Console and ConPTY, and also explicitly run the legacy opt-out.
The required default PTY oracles inspect actual Home/End visible cells,
a post-Box owner frame with **exactly two Spacer nodes**, real wheel input,
80x24→52x16 terminal resize and complete terminal restoration. Separate
rapid-history PTY tests exercise 24 real SGR wheel events across estimated
history and validate that the oldest projected retained ID changes.
ASan/UBSan and independent LSan retain the 256-entry height index,
variable-height GFM growth/shrink, allocator rollback and bounded-host gates.

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
| Up / Page Up / mouse wheel up | Scroll three terminal text rows toward history (legacy opt-out: one older retained message) |
| Down / Page Down / mouse wheel down | Scroll three terminal text rows toward new content (legacy opt-out: one newer retained message) |
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

The underlying Tango, Markdown consumer, and bounded transcript work were
merged via [#265](https://github.com/qigao/stun/pull/265),
[#268](https://github.com/qigao/stun/pull/268) and
[#269](https://github.com/qigao/stun/pull/269).
This default-policy change is separately qualified against that merged base.

Tracking: [Streaming contract #266](https://github.com/qigao/stun/issues/266)
and [physical macOS PTY Home diagnosis #270](https://github.com/qigao/stun/issues/270).
Issue #270 remains independent of enabling the opt-in-reversible virtual
presentation policy. Passing repeated PTY sessions does not demonstrate the
root cause of the previous intermittent Home deadline.

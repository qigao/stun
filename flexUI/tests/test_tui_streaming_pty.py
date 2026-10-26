#!/usr/bin/env python3
"""Qualify the real native Tango/FlexUI chat application under a POSIX PTY.

This does not mock the Tango terminal. The child uses the ordinary
tui_app_run() blocking OS-wakeup loop, termios, SIGWINCH, stdout escape
sequences, terminal input and the normal cleanup path.

Bounded capture, absolute deadlines, and process-group cleanup prevent a
hanging UI from blocking CI indefinitely.
"""
import argparse
import codecs
import re
import unicodedata
import errno
import fcntl
import os
import pty
import select
import signal
import struct
import subprocess
import sys
import tempfile
import termios
import time

MAX_CAPTURE = 12 * 1024 * 1024
TIMEOUT = 45.0
# The demo records at most 64 commands and 64 owner frames (plus geometry).
# Reject overflow instead of silently reading only the oldest 16 KiB.
PTY_TRACE_LIMIT = 64 * 1024



class VisibleTerminal:
    """Minimal terminal-cell oracle for Tango ANSI diffs; never grep input bytes."""

    def __init__(self, rows, cols):
        self.rows, self.cols = rows, cols
        self.cells = [[" "] * cols for _ in range(rows)]
        self.y = self.x = 0
        self.saved = (0, 0)
        self.state = "text"
        self.csi = bytearray()
        self.utf8 = codecs.getincrementaldecoder("utf-8")("replace")

    def resize(self, rows, cols):
        cropped = [[" "] * cols for _ in range(rows)]
        for y in range(min(self.rows, rows)):
            cropped[y][:min(self.cols, cols)] = self.cells[y][:min(self.cols, cols)]
        self.rows, self.cols, self.cells = rows, cols, cropped
        self.y = min(self.y, rows - 1)
        self.x = min(self.x, cols - 1)

    def _put(self, ch):
        width = 0 if unicodedata.combining(ch) else (
            2 if unicodedata.east_asian_width(ch) in "WF" else 1)
        if not width:
            return
        if self.x >= self.cols:
            self.x, self.y = 0, min(self.rows - 1, self.y + 1)
        if self.x < self.cols:
            self.cells[self.y][self.x] = ch
            if width == 2 and self.x + 1 < self.cols:
                self.cells[self.y][self.x + 1] = " "
        self.x += width

    def _escape(self, final):
        raw = self.csi.decode("ascii", "replace")
        if raw.startswith("?"):
            if raw == "?1049" and final == "h":
                self.cells = [[" "] * self.cols for _ in range(self.rows)]
                self.x = self.y = 0
            return
        values = raw.split(";")

        def arg(index, default=1):
            if index < len(values) and values[index].isdigit():
                return int(values[index]) or default
            return default

        if final in "Hf":
            self.y = max(0, min(self.rows - 1, arg(0) - 1))
            self.x = max(0, min(self.cols - 1, arg(1) - 1))
        elif final == "G":
            self.x = max(0, min(self.cols - 1, arg(0) - 1))
        elif final == "A":
            self.y = max(0, self.y - arg(0))
        elif final == "B":
            self.y = min(self.rows - 1, self.y + arg(0))
        elif final == "C":
            self.x = min(self.cols - 1, self.x + arg(0))
        elif final == "D":
            self.x = max(0, self.x - arg(0))
        elif final == "J" and arg(0, 0) == 2:
            self.cells = [[" "] * self.cols for _ in range(self.rows)]
        elif final == "K":
            mode = arg(0, 0)
            start, end = (0, self.cols) if mode == 2 else (
                (0, self.x + 1) if mode == 1 else (self.x, self.cols))
            self.cells[self.y][start:end] = [" "] * (end - start)

    def feed(self, data):
        for byte in data:
            if self.state == "esc":
                if byte == ord("["):
                    self.csi.clear()
                    self.state = "csi"
                elif byte == ord("]"):
                    self.state = "osc"
                else:
                    if byte == ord("7"):
                        self.saved = (self.y, self.x)
                    elif byte == ord("8"):
                        self.y, self.x = self.saved
                    self.state = "text"
                continue
            if self.state == "csi":
                if 0x40 <= byte <= 0x7e:
                    self._escape(chr(byte))
                    self.state = "text"
                elif len(self.csi) < 128:
                    self.csi.append(byte)
                else:
                    self.state = "text"
                continue
            if self.state == "osc":
                if byte == 7:
                    self.state = "text"
                elif byte == 27:
                    self.state = "osc_esc"
                continue
            if self.state == "osc_esc":
                self.state = "text" if byte == ord("\\") else "osc"
                continue
            if byte == 27:
                self.state = "esc"
            elif byte == 13:
                self.x = 0
            elif byte == 10:
                self.y = min(self.rows - 1, self.y + 1)
            elif byte == 8:
                self.x = max(0, self.x - 1)
            elif byte >= 32:
                for ch in self.utf8.decode(bytes((byte,))):
                    self._put(ch)

    def status(self):
        for row in reversed(self.cells):
            line = "".join(row)
            if "Retained " in line:
                return line.strip()
        return ""

    def retained(self):
        match = re.search(r"Retained (\d+)/12", self.status())
        return int(match.group(1)) if match else 0

    def mode(self):
        status = self.status()
        if " | scrollback | " in status:
            return "scrollback"
        if " | tail | " in status:
            return "tail"
        return None

    def geometry(self):
        match = re.search(r"\| (\d+)x(\d+)(?: \||$)", self.status())
        return (int(match.group(1)), int(match.group(2))) if match else None

    def has_header(self):
        return any("FlexUI" in "".join(row) and
                   "Streaming" in "".join(row) for row in self.cells)


def require(ok, detail):
    if not ok:
        raise AssertionError(detail)


def read_owner_trace_lines(path):
    """Read all bounded PTY evidence, including the latest owner frame.

    Reading only the first 16 KiB hid later commands under wheel bursts.
    Do not silently truncate this independent input/Box diagnostic stream.
    It never substitutes for the physical terminal-cell oracle.
    """
    with open(path, "rb") as trace:
        captured = trace.read(PTY_TRACE_LIMIT + 1)
    require(len(captured) <= PTY_TRACE_LIMIT,
            "independent PTY trace exceeded 64 KiB; evidence incomplete")
    return captured.splitlines()


def winsize(fd, rows, cols):
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))


def started_as_tty():
    master, slave = pty.openpty()
    winsize(slave, 24, 80)
    return master, slave


def run_test(executable, early_home=False, home_burst=False,
             virtual_spacers=False, legacy_message_scroll=False):
    master, slave = started_as_tty()
    original_termios = termios.tcgetattr(slave)
    output = bytearray()
    screen = VisibleTerminal(24, 80)
    overall_deadline = time.monotonic() + TIMEOUT
    child = None
    # The optional test-only pipe prevents the worker's first BEGIN until
    # the parent has observed genuine Home navigation through a real PTY.
    # It does not fake terminal input or alter UI event decoding.
    gate_read, gate_write = os.pipe() if early_home else (None, None)
    # Independent owner-input evidence. The CI-only demo appends the actual
    # key command when Tango's existing event mapper invokes on_key().
    trace_fd, input_trace = tempfile.mkstemp(prefix="stun-real-pty-input-")
    os.close(trace_fd)

    def pump(wait=0.1):
        readable, _, _ = select.select([master], [], [], wait)
        if not readable:
            return
        try:
            chunk = os.read(master, 65536)
        except OSError as exc:
            if exc.errno == errno.EIO:
                return  # A PTY master signals EOF with EIO on Linux
            raise
        if chunk:
            require(len(output) + len(chunk) <= MAX_CAPTURE,
                    "terminal output exceeded bounded capture")
            output.extend(chunk)
            screen.feed(chunk)

    def until(label, predicate, seconds):
        deadline = min(time.monotonic() + seconds, overall_deadline)
        while time.monotonic() < deadline:
            if predicate():
                return
            pump(0.08)
            if child.poll() is not None:
                for _ in range(3):
                    pump(0.01)
                raise AssertionError(
                    "%s: application exited early (code %r)" %
                    (label, child.returncode))
        raise AssertionError("%s: PTY output deadline exceeded" % label)

    def send(data):
        n = os.write(master, data)
        require(n == len(data), "short PTY input write")

    def require_last_owner_frame(command, following):
        # Post-Box owner evidence is separate from physical VT/cell evidence.
        # A successful input write or command trace can never substitute
        # for the independent screen.mode() assertion made by the caller.
        lines = read_owner_trace_lines(input_trace)
        frames = [line for line in lines if line.startswith(b"owner_view_epoch=")]
        require(frames, "missing independent owner post-layout frame evidence")
        last = frames[-1]
        expected = ("command=%d following=%d " %
                    (command, int(following))).encode("ascii")
        require(expected in last,
                "owner post-layout navigation state mismatch: %r" % last)
        # This is the first owned *cell* observation, before Tango serializes
        # its ANSI delta. The existing screen.mode() check remains the only
        # physical-terminal acceptance, and cannot be bypassed by this trace.
        cell_match = re.search(rb"cell_mode=(-?\d+) cell_row=(-?\d+)", last)
        require(cell_match is not None,
                "missing post-Box cell-buffer provenance: %r" % last)
        cell_mode, cell_row = int(cell_match[1]), int(cell_match[2])
        require(cell_mode == (0 if following else 1) and cell_row >= 0,
                "owner cell-buffer mode differs from navigation: %r" % last)
        # Default demo mode now owns two virtual Spacer nodes. Validate
        # their real owner/Box geometry even in ordinary-rate PTY sessions,
        # not only in the rapid explicit virtual-history stress case.
        if not legacy_message_scroll:
            geometry = [line for line in lines
                        if line.startswith(b"virtual_geometry epoch=")]
            require(geometry, "no virtual geometry owner-frame record")
            last_geometry = geometry[-1]
            require(b"enabled=1" in last_geometry,
                    "virtual spacer path not enabled: %r" % last_geometry)
            sample = re.search(
                rb"visible=(\d+) children=(\d+) "
                rb"leading=([\d.]+) trailing=([\d.]+)",
                last_geometry)
            require(sample is not None,
                    "invalid virtual owner geometry: %r" % last_geometry)
            visible, children = int(sample[1]), int(sample[2])
            require(0 <= visible <= 5 and
                    (children == 0 if visible == 0
                     else children == visible + 2),
                    "unbounded/incorrect virtual Box children: %r" %
                    last_geometry)

    def last_owner_epoch():
        records = read_owner_trace_lines(input_trace)
        views = [line for line in records
                 if line.startswith(b"owner_view_epoch=")]
        if not views:
            return -1
        match = re.search(rb"owner_view_epoch=(\d+)", views[-1])
        return int(match.group(1)) if match else -1

    def last_virtual_first():
        records = read_owner_trace_lines(input_trace)
        geometry = [line for line in records
                    if line.startswith(b"virtual_geometry epoch=")]
        if not geometry:
            return 0
        match = re.search(rb"first=(\d+)", geometry[-1])
        return int(match.group(1)) if match else 0

    def mode_matches(original, restored):
        return original == restored

    try:
        def make_controlling_terminal():
            # start_new_session() runs setsid() first, so the PTY slave is
            # eligible to become the foreground controlling terminal.
            fcntl.ioctl(0, termios.TIOCSCTTY, 0)

        environment = {
            **os.environ, "TERM": "xterm-256color",
            "COLORTERM": "truecolor",
            "STUN_TUI_PTY_TERMIOS_CHECK": "1",
            "STUN_TUI_PTY_INPUT_TRACE": input_trace,
            "LC_ALL": ("en_US.UTF-8" if sys.platform == "darwin"
                       else "C.UTF-8")
        }
        if early_home:
            environment["STUN_TUI_PTY_BEGIN_GATE_FD"] = str(gate_read)
        argv = [executable]
        if legacy_message_scroll:
            argv.append("--legacy-message-scroll")
        if virtual_spacers:
            # The default demo already uses virtual scrolling. Retain the
            # explicit compatibility alias as an additional CLI admission
            # check and add the CI-only rapid worker for deep wheel probes.
            # Normal sessions still use the genuine 28ms concurrent worker.
            argv.extend(["--experimental-virtual-spacers",
                         "--ci-rapid-producer"])
        child = subprocess.Popen(
            argv, stdin=slave, stdout=slave, stderr=slave,
            start_new_session=True,
            preexec_fn=make_controlling_terminal,
            close_fds=True,
            pass_fds=(gate_read,) if early_home else (),
            env=environment)
        if gate_read is not None:
            os.close(gate_read)
            gate_read = None

        until("alternate screen and raw terminal entry",
              lambda: b"\x1b[?1049h" in output and
                      b"\x1b[?2004h" in output and
                      b"\x1b[?1006h" in output, 8.0)
        raw_mode = termios.tcgetattr(slave)
        require(not (raw_mode[3] & termios.ICANON) and
                not (raw_mode[3] & termios.ECHO),
                "application did not enter real terminal raw input mode")

        # A real display frame must render user-visible text, not just
        # emit the terminal-control enter sequence.
        until("native rendered header",
              lambda: screen.has_header(), 6.0)
        # ANSI rendering is incremental: the title may arrive while the
        # first frame's footer has not yet been painted.
        until("initial visible tail status and terminal size",
              lambda: screen.mode() == "tail"
              and screen.geometry() == (80, 24), 6.0)
        if early_home:
            require(screen.retained() == 0,
                    "worker admitted BEGIN before physical Home gate")

        # The worker is publishing concurrently in the default test.
        # In the gated test it cannot publish until Home reaches UI Owner
        # and the independent cell oracle observes scrollback. Use genuine
        # PTY bytes and preserve the existing strict 5-second deadline.
        send(b"\x1b[H")
        until("Home changes visible terminal state to scrollback",
              lambda: screen.mode() == "scrollback", 5.0)
        require_last_owner_frame(5, False)
        if early_home:
            # Assert BOTH independent command admission and the rendered
            # footer before releasing the producer. Raw VT grep is never
            # used in place of actual visible terminal state.
            with open(input_trace, "rb") as admission:
                admitted = admission.read(4096)
            require(b"command=5 following=1 retained=0" in admitted,
                    "Home did not reach UI Owner before first BEGIN")
            require(os.write(gate_write, b"G") == 1,
                    "could not release first producer BEGIN")
            os.close(gate_write)
            gate_write = None
            until("first BEGIN preserves early Home scrollback intent",
                  lambda: screen.retained() >= 1 and
                  screen.mode() == "scrollback", 12.0)

        # With the user looking at history, new worker messages must not
        # hide the ability to return to the newest window via END.
        send(b"\x1b[F")
        until("End changes visible terminal state to tail mode",
              lambda: screen.mode() == "tail", 5.0)
        require_last_owner_frame(4, True)

        # Observe at least six admitted messages *on the rendered screen*.
        # A fixed sleep falsely assumes the CI host can render five windows
        # at a particular speed; wheel-up with <= 5 is a legal no-op.
        until("six rendered message admissions before wheel",
              lambda: screen.retained() >= 6 and screen.mode() == "tail"
              and screen.geometry() == (80, 24), 18.0)

        if home_burst:
            # Interleave actual HOME and END under an ACTIVE Markdown
            # producer, after multiple messages are genuinely rendered.
            # Some Home CSI bytes arrive in separate PTY writes. Every
            # command MUST reach owner state AND physical visible cells;
            # owner frame coalescing is fine, dropping an input is not.
            for round_no in range(3):
                if round_no % 2 == 0:
                    send(b"\x1b[")
                    send(b"H")
                else:
                    send(b"\x1b[H")
                until("concurrent Home %d visible scrollback" % round_no,
                      lambda: screen.mode() == "scrollback", 5.0)
                require_last_owner_frame(5, False)
                send(b"\x1b[F")
                until("concurrent End %d visible tail" % round_no,
                      lambda: screen.mode() == "tail", 5.0)
                require_last_owner_frame(4, True)

            # Race a *real kernel SIGWINCH* with an unfinished Home CSI
            # prefix while the normal worker is publishing Markdown.
            # A signal may interrupt POSIX select()/read(); neither path
            # may mistake EINTR for the 10ms ambiguity timeout and discard
            # ESC [. This complements, but does not replace, the native
            # deterministic EINTR regression. A physically visible Home
            # within the original five-second limit is still required.
            send(b"\x1b[")
            os.kill(child.pid, signal.SIGWINCH)
            send(b"H")
            until("fragmented Home with SIGWINCH visible scrollback",
                  lambda: screen.mode() == "scrollback", 5.0)
            require_last_owner_frame(5, False)
            send(b"\x1b[F")
            until("End after interrupted Home visible tail",
                  lambda: screen.mode() == "tail", 5.0)
            require_last_owner_frame(4, True)

        # Drive one native mouse wheel event. The SGR parser is invoked from
        # tui_terminal_poll(), not from a test-only decoder entry point.
        prior_status = screen.status()
        send(b"\x1b[<64;3;3M")  # wheel-up at column 3, row 3
        until("SGR wheel-up changes visible scroll state",
              lambda: screen.mode() == "scrollback"
              and screen.status() != prior_status, 8.0)
        if not legacy_message_scroll:
            # Every default ordinary PTY session now verifies that a real
            # wheel enters the virtual Box geometry path, not only changes
            # the footer text.
            require_last_owner_frame(2, False)

        if virtual_spacers:
            # This extra coordinate-to-window test uses the explicitly
            # CI-only rapid producer. Wait for ALL 48 settled messages so
            # the tail-first and wheel-first IDs belong to the SAME stable
            # 12-message retained snapshot. Merely seeing 12 slots full
            # while the producer keeps evicting would compare two different
            # generations: the tail can advance past a correct wheel anchor
            # before the screen oracle samples it. Ordinary-rate PTY/Home
            # concurrent tests remain separate; none of their 5s deadlines
            # or native input assertions are weakened.
            def stable_retained_history():
                matched = re.search(r"settled=(\d+)", screen.status())
                return (screen.retained() == 12 and matched is not None
                        and int(matched.group(1)) == 48)

            until("all 48 rapid messages are settled for stable scroll map",
                  stable_retained_history, 18.0)
            previous_epoch = last_owner_epoch()
            send(b"\x1b[F")
            until("virtual End owner frame after full retention",
                  lambda: screen.mode() == "tail" and
                  last_owner_epoch() > previous_epoch, 5.0)
            first_tail = last_virtual_first()
            require(first_tail > 0,
                    "missing first projected message at virtual tail")
            previous_epoch = last_owner_epoch()

            # One burst of actual SGR wheel events now traverses the
            # previous offscreen Spacer. The UI Owner must reproject
            # *older retained messages*, not merely change footer text.
            # Keep actual Home/End and 52x16 terminal restoration checks.
            send(b"\x1b[<64;3;3M" * 24)
            until("real wheel coordinate rewindows retained virtual hosts",
                  lambda: screen.mode() == "scrollback" and
                  last_owner_epoch() > previous_epoch and
                  0 < last_virtual_first() < first_tail, 8.0)
            require_last_owner_frame(2, False)

        # Resize an actual PTY, deliver SIGWINCH to the child with a genuine
        # tty controlling process group, and require another terminal frame.
        screen.resize(16, 52)
        winsize(slave, 16, 52)
        os.kill(child.pid, signal.SIGWINCH)
        until("PTY SIGWINCH applies 52x16 UI geometry",
              lambda: screen.geometry() == (52, 16)
              and screen.mode() == "scrollback"
              and screen.has_header(), 6.0)
        require(child.poll() is None, "application exited during resize")

        if home_burst:
            # Exercise the same strict physical Home/End oracle AFTER a real
            # SIGWINCH while Markdown and the scroll viewport have reflowed.
            # A queued resize may coincide with a partial CSI read; neither
            # owner command provenance nor Box-cell evidence substitutes for
            # the actual 52x16 PTY screen.
            send(b"\x1b[F")
            until("post-resize End displays tail",
                  lambda: screen.mode() == "tail"
                  and screen.geometry() == (52, 16), 5.0)
            require_last_owner_frame(4, True)
            send(b"\x1b[H")
            until("post-resize Home displays scrollback",
                  lambda: screen.mode() == "scrollback"
                  and screen.geometry() == (52, 16), 5.0)
            require_last_owner_frame(5, False)

        # Graceful input exit is needed to assert actual terminal restoration;
        # SIGTERM would only test abnormal process termination.
        send(b"q")
        deadline = time.monotonic() + 8.0
        while child.poll() is None and time.monotonic() < deadline:
            pump(0.05)
        require(child.poll() is not None, "normal keyboard quit hung")
        for _ in range(8):
            pump(0.01)
        require(child.returncode == 0,
                "application returned nonzero on q: %r" % child.returncode)

        for code in (b"\x1b[?1006l", b"\x1b[?2004l",
                     b"\x1b[?25h", b"\x1b[?1049l"):
            require(code in output, "missing terminal restoration %r" % code)
        require(output.index(b"\x1b[?1049h") <
                output.rindex(b"\x1b[?1049l"),
                "alternate screen left before it was entered")
        # The production tui_app_run() path must restore the complete
        # original termios before returning. The CI-only in-process oracle
        # attests to that exact comparison while the PTY remains attached.
        # Darwin revokes the slave tty on session-leader exit: a parent-side
        # post-exit tcgetattr() raises ENOTTY even after correct restoration.
        require(b"TANGO_PTY_TERMIOS_RESTORED" in output,
                "missing in-process complete termios restoration evidence")
        if sys.platform != "darwin":
            require(mode_matches(original_termios, termios.tcgetattr(slave)),
                    "parent-observed termios mismatch after normal exit")
        print("native POSIX PTY chat passed: keyboard, SGR mouse, "
              "SIGWINCH 52x16 UI, termios restoration and terminal controls; "
              "early_home=%d, home_burst=%d, default_virtual=%d, "
              "rapid_virtual=%d, captured=%d bytes" %
              (int(early_home), int(home_burst),
               int(not legacy_message_scroll), int(virtual_spacers),
               len(output)))
    except Exception:
        print("PTY visible status: %r; retained=%d; mode=%r; "
              "geometry=%r; parser_state=%s" %
              (screen.status(), screen.retained(), screen.mode(),
               screen.geometry(), screen.state), file=sys.stderr)
        print("PTY output diagnostic (tail): %r" % bytes(output[-1400:]),
              file=sys.stderr)
        print("PTY raw output bytes=%d, literal scrollback chunks=%d, "
              "child_alive=%s" %
              (len(output), output.count(b"scrollback"),
               child is not None and child.poll() is None),
              file=sys.stderr)
        try:
            with open(input_trace, "rb") as trace:
                trace.seek(0, os.SEEK_END)
                total_bytes = trace.tell()
                trace.seek(max(0, total_bytes - 8192), os.SEEK_SET)
                latest = trace.read(8192)
            print("PTY independent owner-input trace (bytes=%d; latest 8 KiB): %r" %
                  (total_bytes, latest), file=sys.stderr)
        except OSError as exc:
            print("PTY owner-input trace unavailable: %s" % exc,
                  file=sys.stderr)
        raise
    finally:
        if child is not None and child.poll() is None:
            # The demo has no subprocess children, but owns its process
            # group. macOS can reject killpg after session teardown; fall
            # back to signaling the exact child PID and always reap it.
            try:
                os.killpg(child.pid, signal.SIGTERM)
            except (PermissionError, ProcessLookupError):
                if child.poll() is None:
                    child.terminate()
            try:
                child.wait(timeout=1.5)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(child.pid, signal.SIGKILL)
                except (PermissionError, ProcessLookupError):
                    if child.poll() is None:
                        child.kill()
                child.wait(timeout=2.0)
        if gate_read is not None:
            os.close(gate_read)
        if gate_write is not None:
            os.close(gate_write)
        os.close(master)
        os.close(slave)
        try:
            os.unlink(input_trace)
        except OSError:
            pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable")
    parser.add_argument("--early-home", action="store_true",
                        help="gate first worker BEGIN until real PTY Home is visible")
    parser.add_argument("--home-burst", action="store_true",
                        help="interleave strict real Home/End under active Markdown bursts")
    parser.add_argument("--virtual-spacers", action="store_true",
                        help="run deep coordinate-to-window stress with a rapid producer")
    parser.add_argument("--legacy-message-scroll", action="store_true",
                        help="verify the explicit opt-out path under a real PTY")
    args = parser.parse_args()
    require(os.path.isfile(args.executable),
            "missing real streaming demo executable")
    require(not (args.virtual_spacers and args.legacy_message_scroll),
            "virtual stress and legacy opt-out are mutually exclusive")
    run_test(os.path.abspath(args.executable), early_home=args.early_home,
             home_burst=args.home_burst,
             virtual_spacers=args.virtual_spacers,
             legacy_message_scroll=args.legacy_message_scroll)


if __name__ == "__main__":
    main()

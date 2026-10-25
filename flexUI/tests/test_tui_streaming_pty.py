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
import termios
import time

MAX_CAPTURE = 12 * 1024 * 1024
TIMEOUT = 45.0



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


def winsize(fd, rows, cols):
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))


def started_as_tty():
    master, slave = pty.openpty()
    winsize(slave, 24, 80)
    return master, slave


def run_test(executable):
    master, slave = started_as_tty()
    original_termios = termios.tcgetattr(slave)
    output = bytearray()
    screen = VisibleTerminal(24, 80)
    overall_deadline = time.monotonic() + TIMEOUT
    child = None

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

    def mode_matches(original, restored):
        return original == restored

    try:
        def make_controlling_terminal():
            # start_new_session() runs setsid() first, so the PTY slave is
            # eligible to become the foreground controlling terminal.
            fcntl.ioctl(0, termios.TIOCSCTTY, 0)

        child = subprocess.Popen(
            [executable], stdin=slave, stdout=slave, stderr=slave,
            start_new_session=True,
            preexec_fn=make_controlling_terminal,
            close_fds=True,
            env={**os.environ, "TERM": "xterm-256color",
                 "COLORTERM": "truecolor",
                 "LC_ALL": ("en_US.UTF-8" if sys.platform == "darwin"
                            else "C.UTF-8")})

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

        # The worker is publishing concurrently. Browse with a POSIX
        # terminal HOME sequence and verify the ordinary owner event map
        # caused a visible status transition.
        send(b"\x1b[H")
        until("Home changes visible terminal state to scrollback",
              lambda: screen.mode() == "scrollback", 5.0)

        # With the user looking at history, new worker messages must not
        # hide the ability to return to the newest window via END.
        send(b"\x1b[F")
        until("End changes visible terminal state to tail mode",
              lambda: screen.mode() == "tail", 5.0)

        # Observe at least six admitted messages *on the rendered screen*.
        # A fixed sleep falsely assumes the CI host can render five windows
        # at a particular speed; wheel-up with <= 5 is a legal no-op.
        until("six rendered message admissions before wheel",
              lambda: screen.retained() >= 6 and screen.mode() == "tail"
              and screen.geometry() == (80, 24), 18.0)

        # Drive one native mouse wheel event. The SGR parser is invoked from
        # tui_terminal_poll(), not from a test-only decoder entry point.
        prior_status = screen.status()
        send(b"\x1b[<64;3;3M")  # wheel-up at column 3, row 3
        until("SGR wheel-up changes visible scroll state",
              lambda: screen.mode() == "scrollback"
              and screen.status() != prior_status, 8.0)

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
        require(mode_matches(original_termios, termios.tcgetattr(slave)),
                "termios flags were not fully restored after normal exit")
        print("native POSIX PTY chat passed: keyboard, SGR mouse, "
              "SIGWINCH 52x16 UI, termios restoration and terminal controls; "
              "captured=%d bytes" %
              len(output))
    except Exception:
        print("PTY visible status: %r; retained=%d; mode=%r; "
              "geometry=%r; parser_state=%s" %
              (screen.status(), screen.retained(), screen.mode(),
               screen.geometry(), screen.state), file=sys.stderr)
        print("PTY output diagnostic (tail): %r" % bytes(output[-1400:]),
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
        os.close(master)
        os.close(slave)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable")
    args = parser.parse_args()
    require(os.path.isfile(args.executable),
            "missing real streaming demo executable")
    run_test(os.path.abspath(args.executable))


if __name__ == "__main__":
    main()

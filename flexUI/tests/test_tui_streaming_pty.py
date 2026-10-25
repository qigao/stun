#!/usr/bin/env python3
"""Qualify the real native Tango/FlexUI chat application under a POSIX PTY.

This does not mock the Tango terminal. The child uses the ordinary
tui_app_run() blocking OS-wakeup loop, termios, SIGWINCH, stdout escape
sequences, terminal input and the normal cleanup path.

Bounded capture, absolute deadlines, and process-group cleanup prevent a
hanging UI from blocking CI indefinitely.
"""
import argparse
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
TIMEOUT = 32.0


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

    def until(label, predicate, seconds):
        deadline = time.monotonic() + seconds
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
                 "COLORTERM": "truecolor", "LC_ALL": "C.UTF-8"})

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
              lambda: b"FlexUI" in output and b"Streaming" in output, 6.0)

        # The worker is publishing concurrently. Browse with a POSIX
        # terminal HOME sequence and verify the ordinary owner event map
        # caused a visible status transition.
        before = len(output)
        send(b"\x1b[H")
        until("Home changes actual UI to scrollback",
              lambda: b"scrollback" in output[before:], 5.0)

        # With the user looking at history, new worker messages must not
        # hide the ability to return to the newest window via END.
        before = len(output)
        send(b"\x1b[F")
        until("End changes actual UI to tail mode",
              lambda: b"tail" in output[before:], 5.0)

        # Drive one native mouse wheel event. The SGR parser is invoked from
        # tui_terminal_poll(), not from a test-only decoder entry point.
        before = len(output)
        send(b"\x1b[<64;3;3M")  # wheel-up at column 3, row 3
        until("SGR wheel-up redraw",
              lambda: b"scrollback" in output[before:], 5.0)

        # Resize an actual PTY, deliver SIGWINCH to the child with a genuine
        # tty controlling process group, and require another terminal frame.
        before = len(output)
        winsize(slave, 16, 52)
        os.kill(child.pid, signal.SIGWINCH)
        until("PTY SIGWINCH reflow",
              lambda: len(output) - before >= 80, 5.0)
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
        print("native POSIX PTY chat passed: keyboard, SGR mouse, SIGWINCH, "
              "termios restoration and terminal controls; captured=%d bytes" %
              len(output))
    except Exception:
        print("PTY output diagnostic (tail): %r" % bytes(output[-1400:]),
              file=sys.stderr)
        raise
    finally:
        if child is not None and child.poll() is None:
            try:
                os.killpg(child.pid, signal.SIGTERM)
                child.wait(timeout=1.5)
            except (ProcessLookupError, subprocess.TimeoutExpired):
                if child.poll() is None:
                    os.killpg(child.pid, signal.SIGKILL)
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

#!/usr/bin/env python3
"""Real XTest-to-GLFW X11 pointer grab acceptance for the existing desktop editor.

This script is invoked by the ORIGINAL Stun native OpenGL/X11 GUI job. It
launches the actual linked production editor, moves the X server pointer
OUTSIDE that window during an active XGrabPointer and releases the real XTest
button there. There are NO app.dispatch_event() shortcuts or fake grab flags.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import select
import subprocess
import sys
import time

from Xlib import X, display
from Xlib.ext import xtest

import gui_native_e2e as gui


class NativeInputFailure(RuntimeError):
    pass


class NativeOutput:
    """Bounded, line-buffered stdout handshake; never block on readline()."""

    def __init__(self, process):
        self.process = process
        self.pending = ""
        self.history = []

    def until(self, prefix, seconds):
        deadline = time.monotonic() + seconds
        fd = self.process.stdout.fileno()
        while time.monotonic() < deadline:
            while "\n" in self.pending:
                line, self.pending = self.pending.split("\n", 1)
                line = line.rstrip("\r")
                self.history.append(line)
                print("NATIVE:", line, flush=True)
                if line.startswith(prefix):
                    return line
            ready, _, _ = select.select([fd], [], [], max(
                .0, min(.10, deadline - time.monotonic())))
            if ready:
                chunk = os.read(fd, 32768)
                if chunk:
                    self.pending += chunk.decode("utf-8", "replace")
                    continue
            if self.process.poll() is not None:
                raise NativeInputFailure(
                    f"native host exited {self.process.returncode} before {prefix}: "
                    + " | ".join(self.history[-9:]))
        raise NativeInputFailure(
            f"native host did not report {prefix} within the bounded protocol: "
            + " | ".join(self.history[-9:]))


def real_x11_grab_status(disp, window):
    """Read the X SERVER grab result from an independent Xlib client."""
    grab = window.grab_pointer(
        owner_events=False,
        event_mask=X.PointerMotionMask | X.ButtonReleaseMask,
        pointer_mode=X.GrabModeAsync,
        keyboard_mode=X.GrabModeAsync,
        confine_to=X.NONE,
        cursor=X.NONE,
        time=X.CurrentTime)
    disp.sync()
    if grab.status == X.GrabSuccess:
        # This probe is itself a real server-side grab; never leave it live.
        disp.ungrab_pointer(X.CurrentTime)
        disp.sync()
    return grab.status


def wait_for_real_window(disp, deadline):
    while time.monotonic() < deadline:
        windows = gui.find_titled_windows(disp, gui.TITLE)
        if len(windows) == 1:
            return windows[0]
        if len(windows) > 1:
            raise NativeInputFailure("multiple identical native editor windows")
        time.sleep(.02)
    raise NativeInputFailure("no visible native GLFW/X11 editor window was mapped")


def run(exe):
    if not exe.is_file():
        raise NativeInputFailure(f"missing real production executable: {exe}")
    if not os.environ.get("DISPLAY"):
        raise NativeInputFailure("real X11 DISPLAY required; no headless bypass")

    disp = display.Display(os.environ["DISPLAY"])
    proc = None
    pressed = False
    try:
        proc = subprocess.Popen(
            [str(exe), "--smoke-x11-input"],
            cwd=str(exe.parent), stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, bufsize=0,
            close_fds=True)
        output = NativeOutput(proc)
        ready = output.until("STUN_X11_INPUT_READY ", 8.0)
        fields = ready.split()
        if len(fields) != 5:
            raise NativeInputFailure(f"invalid native host CSS geometry: {ready}")
        gx, gy, gw, gh = (float(value) for value in fields[1:])
        if not (0 <= gx < 200 and 40 < gy < 350 and
                720 <= gw <= 900 and 150 <= gh <= 170):
            raise NativeInputFailure(f"unreasonable actual graph extent: {ready}")

        window = wait_for_real_window(disp, time.monotonic() + 4.0)
        placement = {}
        gui.place_window(
            disp, window, placement, deadline=time.monotonic() + 4.0)
        ox, oy, width, height = gui.window_rect(disp, window)
        screen = disp.screen().root.get_geometry()
        if width != gui.WIDTH or height != gui.HEIGHT:
            raise NativeInputFailure("the real GLFW window size is not the known editor size")
        outside_x = ox + width + 24
        outside_y = oy + int(gy) + 20
        if not (0 <= outside_x < screen.width and
                0 <= outside_y < screen.height):
            raise NativeInputFailure("XTest outside-window destination is off the X11 screen")

        # Reject a stale capture from any earlier window before the gesture.
        if real_x11_grab_status(disp, disp.screen().root) != X.GrabSuccess:
            raise NativeInputFailure("X server already has a foreign pointer grab")

        # This is real XTest input through the X server. GLFW must deliver
        # MouseDown to GraphCanvas and request the platform-native grab.
        disp.set_input_focus(window, X.RevertToParent, X.CurrentTime)
        xtest.fake_input(
            disp, X.MotionNotify, x=ox + int(gx) + 20,
            y=oy + int(gy) + 125)
        disp.sync()
        time.sleep(.06)
        xtest.fake_input(disp, X.ButtonPress, 1)
        disp.sync()
        pressed = True
        output.until("STUN_X11_INPUT_GRABBED", 5.0)

        # A *different client* must be denied a competing real X server
        # grab, not merely observe a local gCanvas capture boolean.
        contention = real_x11_grab_status(disp, disp.screen().root)
        if contention != X.AlreadyGrabbed:
            raise NativeInputFailure(
                f"X11 native grab is absent or nonexclusive; competing status={contention}")

        # Move the XTest device OUTSIDE the original native client while
        # it owns the X server grab. Release outside, not back inside.
        xtest.fake_input(disp, X.MotionNotify, x=outside_x, y=outside_y)
        disp.sync()
        observed = disp.screen().root.query_pointer()
        if (observed.root_x != outside_x or observed.root_y != outside_y or
                outside_x < ox + width):
            raise NativeInputFailure("XTest root pointer never crossed the actual window edge")
        time.sleep(.08)
        xtest.fake_input(disp, X.ButtonRelease, 1)
        disp.sync()
        pressed = False

        # The producer's own Box/GraphCanvas owner only reports success
        # after processing this outside-window MouseUp: selection must
        # contain BOTH original nodes, no change to graph/Undo, and the
        # native grab must already be released before plugin shutdown.
        output.until("STUN_X11_INPUT_RELEASED", 5.0)
        if real_x11_grab_status(disp, disp.screen().root) != X.GrabSuccess:
            raise NativeInputFailure("X server retained grab after outside-window MouseUp")
        output.until("STUN_X11_INPUT_PASS:", 5.0)
        if proc.wait(timeout=5) != 0:
            raise NativeInputFailure("native editor failed despite reported X11 input completion")
        print("X11 OUTSIDE INPUT PASS: XTest ButtonRelease outside the native window "
              "reached the original Box, X server denied competing grab during the "
              "gesture and accepted it after real release", flush=True)
    finally:
        if pressed:
            try:
                xtest.fake_input(disp, X.ButtonRelease, 1)
                disp.sync()
            except Exception:
                pass
        if proc is not None and proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait(timeout=2)
        disp.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    args = parser.parse_args()
    try:
        run(args.exe.resolve())
    except (NativeInputFailure, OSError, ValueError,
            subprocess.SubprocessError) as error:
        print("X11 OUTSIDE INPUT FAIL:", error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Strict real X11 window screenshot and mouse E2E for Stun Desktop Editor.

Runs the REAL executable in visible mode; never --smoke or in-process events.
Pixel evidence is captured from X11, never a simulated renderer. Requires
python3-xlib and Pillow on an actual X11 display (Xvfb supported).
Optional offline tesseract OCR; no remote service or HTTP provider.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import time

WIDTH, HEIGHT = 928, 608
TITLE = "FlexUI Desktop Editor"
# Exercise root/client coordinate translation in the production application.
WINDOW_ORIGIN = (127, 83)
# Independent client-space contract: 24px shell padding, 680px title,
# 16px gap and a 136x36 Save control. Never derive the oracle from the
# runtime assets under test; a broken asset must not redefine correctness.
EXPECTED_SAVE_BOX = (720, 24, 856, 60)
SAVE_LAYOUT_TOLERANCE = 4


class VisualFailure(RuntimeError):
    def __init__(self, message, *, code="gui_failure"):
        super().__init__(message)
        self.code = code


class OcrMismatch(VisualFailure):
    """A readable frame has not reached the required visible state yet."""

    def __init__(self, message, words):
        super().__init__(message, code="ocr_mismatch")
        self.words = words


def is_button_blue(rgb):
    r, g, b = rgb[:3]
    return (10 <= r <= 145 and 40 <= g <= 198 and
            b >= 165 and b >= g + 27 and b >= r + 72)


def detect_button(frame):
    """Find actual rendered CSS-blue Save surface (not a blind coordinate)."""
    get = frame.load()
    width, height = frame.size
    if width < 480 or height < 320:
        raise VisualFailure("GUI frame is unexpectedly small")
    n, xmin, ymin, xmax, ymax = 0, width, height, -1, -1
    for y in range(0, int(height * .35)):
        for x in range(int(width * .45), width - 8):
            if is_button_blue(get[x, y]):
                n += 1
                xmin, xmax = min(xmin, x), max(xmax, x)
                ymin, ymax = min(ymin, y), max(ymax, y)
    bw, bh = xmax - xmin + 1, ymax - ymin + 1
    if not (n >= 400 and 70 <= bw <= 430 and 20 <= bh <= 88):
        raise VisualFailure(
            f"real Save button not visible: {n} blue pixels, {bw}x{bh} bbox")
    return xmin, ymin, xmax + 1, ymax + 1


def button_layout(box):
    """Compare rendered bounds with the fixed Linux editor layout contract."""
    if len(box) != 4 or not all(type(value) is int for value in box):
        raise VisualFailure("invalid rendered Save bounds", code="layout_mismatch")
    deltas = [actual - expected for actual, expected in zip(box, EXPECTED_SAVE_BOX)]
    return {
        "expected_box": list(EXPECTED_SAVE_BOX),
        "observed_box": list(box),
        "edge_deltas": deltas,
        "tolerance_pixels": SAVE_LAYOUT_TOLERANCE,
        "matches": all(abs(delta) <= SAVE_LAYOUT_TOLERANCE for delta in deltas),
    }


def count_changed(a, b, bounds, threshold=26):
    if a.size != b.size:
        raise VisualFailure("GUI frame geometry changed during screenshot")
    x1, y1, x2, y2 = bounds
    if not (0 <= x1 < x2 <= a.width and 0 <= y1 < y2 <= a.height):
        raise VisualFailure("invalid screenshot comparison area")
    first, second = a.load(), b.load()
    n = 0
    for y in range(y1, y2):
        for x in range(x1, x2):
            if max(abs(int(first[x, y][k]) - int(second[x, y][k]))
                   for k in range(3)) >= threshold:
                n += 1
    return n


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as file:
        while True:
            part = file.read(1024 * 1024)
            if not part:
                break
            h.update(part)
    return h.hexdigest()


def find_titled_windows(disp, title):
    from Xlib import X
    root = disp.screen().root
    stack, visited, matches = [(root, 0)], set(), []
    while stack:
        window, depth = stack.pop()
        if window.id in visited:
            continue
        visited.add(window.id)
        if len(visited) > 512:
            raise VisualFailure("too many X11 windows to identify unique GUI")
        try:
            name = window.get_wm_name()
            if isinstance(name, bytes):
                name = name.decode("utf-8", "replace")
            if name == title and window.get_attributes().map_state == X.IsViewable:
                matches.append(window)
            if depth < 5:
                stack.extend((child, depth + 1)
                             for child in window.query_tree().children)
        except Exception:
            continue  # Other processes may close windows during traversal.
    return matches


def window_rect(disp, window):
    root = disp.screen().root
    geometry = window.get_geometry()
    # python-xlib's receiver is the destination, its argument the source:
    # Window.translate_coords(self, src_window, src_x, src_y).
    pos = root.translate_coords(window, 0, 0)
    if not pos.same_screen:
        raise VisualFailure("native window is on a different X11 screen")
    return int(pos.x), int(pos.y), int(geometry.width), int(geometry.height)


def place_window(disp, window, report, *, deadline):
    """Send one move request and require the actual client origin to settle."""
    initial = window_rect(disp, window)
    screen = disp.screen().root.get_geometry()
    report.update(initial_rect=list(initial), requested_origin=list(WINDOW_ORIGIN),
                  observed_rect=list(initial),
                  screen_size=[int(screen.width), int(screen.height)])
    x, y = WINDOW_ORIGIN
    if not (x > 0 and y > 0 and x + initial[2] <= screen.width and
            y + initial[3] <= screen.height):
        raise VisualFailure("off-origin GUI does not fit its X11 screen",
                            code="window_placement_failed")
    geometry = window.get_geometry()
    errors = []

    def move_error(error, _request):
        errors.append(str(error))
        return True

    # Configure uses parent-relative outer coordinates; translate only by the
    # required root-space delta, retaining parent offsets and window borders.
    window.configure(x=int(geometry.x) + x - initial[0],
                     y=int(geometry.y) + y - initial[1], onerror=move_error)
    disp.sync()
    if errors:
        raise VisualFailure("X11 rejected the window move: " + errors[0],
                            code="window_placement_failed")
    settle_deadline = min(deadline, time.monotonic() + 2.0)
    while time.monotonic() < settle_deadline:
        observed = window_rect(disp, window)
        report["observed_rect"] = list(observed)
        if observed[2:] != initial[2:]:
            raise VisualFailure("moving the GUI changed its client size",
                                code="window_placement_failed")
        if observed[:2] == WINDOW_ORIGIN:
            return observed
        time.sleep(.05)
    raise VisualFailure(
        f"native GUI did not reach screen origin {list(WINDOW_ORIGIN)}: "
        f"observed {report['observed_rect']}", code="window_placement_failed")


def capture(disp, window, *, expected_rect=None, desktop_path=None):
    from PIL import ImageGrab
    rect = window_rect(disp, window)
    if expected_rect is not None and rect != tuple(expected_rect):
        raise VisualFailure("native GUI moved or resized before capture",
                            code="window_geometry_changed")
    x, y, w, h = rect
    screen = ImageGrab.grab(xdisplay=os.environ["DISPLAY"]).convert("RGB")
    if window_rect(disp, window) != rect:
        raise VisualFailure("native GUI moved or resized during capture",
                            code="window_geometry_changed")
    if x < 0 or y < 0 or x + w > screen.width or y + h > screen.height:
        raise VisualFailure("the real GUI is not fully visible on its X11 screen")
    if desktop_path is not None:
        screen.save(desktop_path)
    return screen.crop((x, y, x + w, y + h))


def mouse(disp, window, x, y, *, click=False, expected_rect=None):
    from Xlib import X
    from Xlib.ext import xtest
    rect = window_rect(disp, window)
    if expected_rect is not None and rect != tuple(expected_rect):
        raise VisualFailure("native GUI moved or resized before mouse input",
                            code="window_geometry_changed")
    ox, oy, w, h = rect
    if not (0 <= x < w and 0 <= y < h):
        raise VisualFailure("mouse target was outside real window")
    disp.set_input_focus(window, X.RevertToParent, X.CurrentTime)
    xtest.fake_input(disp, X.MotionNotify, x=ox + x, y=oy + y)
    disp.sync()
    pointer = window.query_pointer()
    observed_screen = [int(pointer.root_x), int(pointer.root_y)]
    observed_client = [int(pointer.win_x), int(pointer.win_y)]
    if (not pointer.same_screen or pointer.root.id != disp.screen().root.id or
            observed_screen != [ox + x, oy + y] or observed_client != [x, y] or
            window_rect(disp, window) != rect):
        raise VisualFailure("XTest pointer did not reach the real client target",
                            code="mouse_target_mismatch")
    if click:
        xtest.fake_input(disp, X.ButtonPress, 1)
        disp.sync()
        time.sleep(.06)
        xtest.fake_input(disp, X.ButtonRelease, 1)
        disp.sync()
    return {"action": "left_click" if click else "move",
            "window_rect": list(rect), "client": [x, y],
            "screen": [ox + x, oy + y], "observed_screen": observed_screen,
            "observed_client": observed_client}


def request_close(disp, window):
    from Xlib import X, protocol
    protocols = disp.intern_atom("WM_PROTOCOLS")
    delete = disp.intern_atom("WM_DELETE_WINDOW")
    accepts = window.get_wm_protocols()
    if accepts is None or delete not in accepts:
        raise VisualFailure("native window does not support WM_DELETE_WINDOW")
    message = protocol.event.ClientMessage(
        window=window, client_type=protocols,
        data=(32, [delete, X.CurrentTime, 0, 0, 0]))
    window.send_event(message, event_mask=0)
    disp.sync()


def label_starts_with(words, expected):
    """Match a confident leading word sequence without hiding uncertain words.

    Pure punctuation, including detected control borders, does not occupy a
    word position. Every meaningful observation does, even when its confidence
    is invalid or low: discarding an uncertain "Not" could turn failure into
    success. Only the required prefix needs to be readable; clipped trailing
    text does not invalidate an already established prefix.
    """
    if not isinstance(words, list) or not isinstance(expected, str):
        return False
    wanted = re.findall(r"\w+", expected.lower())
    if not wanted:
        return False
    tokens = []
    for word in words:
        if not isinstance(word, dict) or not isinstance(word.get("text"), str):
            return False
        try:
            confidence = float(word.get("confidence"))
        except (TypeError, ValueError):
            confidence = None
        for token in re.findall(r"\w+", word["text"].lower()):
            tokens.append((token, confidence))
    return len(tokens) >= len(wanted) and all(
        token == required and confidence is not None and
        math.isfinite(confidence) and 20 <= confidence <= 100
        for (token, confidence), required in zip(tokens, wanted))


def local_ocr(path, expected):
    """Optional offline OCR, supplemental to real pixel and input evidence."""
    result = subprocess.run(
        ["tesseract", str(path), "stdout", "--psm", "7", "tsv"],
        capture_output=True, text=True, timeout=15, check=False)
    if result.returncode:
        raise VisualFailure("local OCR failed: " + result.stderr[-320:])
    words = []
    for line in result.stdout.splitlines()[1:]:
        fields = line.split("\t", 11)
        if len(fields) != 12:
            continue
        try:
            conf = float(fields[10])
        except ValueError:
            conf = None
        if conf is not None and not math.isfinite(conf):
            conf = None  # Keep JSON valid and the unreadable word in position.
        if fields[11].strip():
            words.append({
                "text": fields[11].strip(), "confidence": conf,
                "box": [int(fields[i]) for i in (6, 7, 8, 9)]
            })
    found = re.findall(r"\w+", " ".join(x["text"] for x in words).lower())
    wanted = re.findall(r"\w+", expected.lower())
    if not label_starts_with(words, expected):
        raise OcrMismatch(
            f"visible OCR {found!r} did not start with confident words {wanted!r}", words)
    return words


def button_ocr(frame, box, path, expected):
    from PIL import Image, ImageOps
    # Exclude the control border while preserving antialiased glyph strokes.
    # Every label uses this same transform; full-window frames and the larger
    # hover/after pixel comparison region remain the visual/layout evidence.
    inner = (box[0] + 2, box[1] + 2, box[2] - 2, box[3] - 2)
    crop = frame.crop(inner)
    crop = ImageOps.invert(ImageOps.autocontrast(ImageOps.grayscale(crop)))
    crop = ImageOps.expand(
        crop.resize((crop.width * 3, crop.height * 3), Image.Resampling.BICUBIC),
        border=20, fill="white")
    crop.save(path)
    return local_ocr(path, expected)


def run(args):
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    metadata = {
        "source_sha": args.source_sha, "binary": str(Path(args.exe).resolve()),
        "title": TITLE, "display": os.environ.get("DISPLAY", ""),
        "visible_window": False,
        "real_mouse_event": False, "result": "FAIL"
    }
    start = time.monotonic()
    disp, process, target = None, None, None
    close_requested = False
    try:
        try:
            from Xlib import display, X
            from PIL import ImageStat
        except ImportError as error:
            raise VisualFailure("python3-xlib and Pillow required for real GUI") from error
        if not os.environ.get("DISPLAY"):
            raise VisualFailure("real DISPLAY required; use xvfb-run or desktop X11")
        if not re.fullmatch("[0-9a-f]{40}", args.source_sha):
            raise VisualFailure("exact 40-character source HEAD required")
        if args.timeout <= 0 or args.minimum_changed < 1:
            raise VisualFailure("timeout and minimum changed pixels must be positive")
        exe = Path(args.exe).resolve(strict=True)
        if not exe.is_file() or not os.access(exe, os.X_OK):
            raise VisualFailure("native GUI executable missing or not executable")
        metadata["binary_sha256"] = sha256_file(exe)
        disp = display.Display()
        prior = [w.id for w in find_titled_windows(disp, TITLE)]
        if prior:
            raise VisualFailure("existing same-title GUI makes window ownership ambiguous")
        with open(out / "application.log", "wb") as log:
            process = subprocess.Popen(
                [str(exe)], cwd=str(exe.parent), stdout=log,
                stderr=subprocess.STDOUT, start_new_session=True)
            metadata["pid"] = process.pid
            deadline = time.monotonic() + args.timeout
            target = None
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise VisualFailure(
                        f"app exited before visible window: {process.returncode}")
                found = [w for w in find_titled_windows(disp, TITLE)
                         if w.id not in prior]
                if len(found) > 1:
                    raise VisualFailure("ambiguous multiple new application windows")
                if found:
                    target = found[0]
                    break
                time.sleep(.05)
            if target is None:
                raise VisualFailure("visible native window not created")
            x, y, width, height = window_rect(disp, target)
            if abs(width - WIDTH) > 8 or abs(height - HEIGHT) > 8:
                raise VisualFailure(f"wrong client geometry: {width}x{height}")
            pid = target.get_full_property(
                disp.intern_atom("_NET_WM_PID"), X.AnyPropertyType)
            if pid is not None and pid.value and int(pid.value[0]) != process.pid:
                raise VisualFailure("window PID belongs to another process")
            metadata.update(visible_window=True, window_id=hex(target.id),
                            window_rect=[x, y, width, height])
            metadata["window_placement"] = {}
            expected_rect = place_window(
                disp, target, metadata["window_placement"], deadline=deadline)
            metadata["window_rect"] = list(expected_rect)
            # GLFW maps the window before XML/controller initialization has
            # necessarily rendered. Wait for actual pixels, not a fixed sleep.
            ready_deadline = min(deadline, time.monotonic() + 6.0)
            ready, last_initial_error = False, "no frame captured"
            last_initial_code = "initial_state_not_reached"
            initial_samples, initial_stddev = 0, 0.0
            while time.monotonic() < ready_deadline:
                last_initial_code = "initial_state_not_reached"
                if process.poll() is not None:
                    raise VisualFailure(
                        f"app exited before rendered Save: {process.returncode}")
                before = capture(disp, target, expected_rect=expected_rect,
                                 desktop_path=out / "desktop_before.png")
                before.save(out / "before.png")
                initial_samples += 1
                variation = max(ImageStat.Stat(before).stddev)
                initial_stddev = max(initial_stddev, variation)
                metadata["initial_frame_samples"] = initial_samples
                metadata["initial_max_stddev"] = round(initial_stddev, 4)
                try:
                    if variation < 8:
                        raise VisualFailure("real GUI screenshot is blank")
                    box = detect_button(before)
                except VisualFailure as exc:
                    last_initial_error = str(exc)
                    time.sleep(.08)
                    continue
                roi = (max(0, box[0] - 5), max(0, box[1] - 3),
                       min(width, box[2] + 5), min(height, box[3] + 3))
                metadata["save_button_box"] = list(box)
                metadata["save_button_layout"] = button_layout(box)
                if args.ocr_initial:
                    try:
                        metadata["initial_ocr"] = button_ocr(
                            before, box, out / "before_button_ocr.png",
                            args.ocr_initial)
                        metadata["initial_ocr_observed"] = metadata["initial_ocr"]
                    except OcrMismatch as exc:
                        last_initial_error = str(exc)
                        metadata["initial_ocr_last_mismatch"] = str(exc)
                        metadata["initial_ocr_observed"] = exc.words
                        time.sleep(.08)
                        continue
                if not metadata["save_button_layout"]["matches"]:
                    last_initial_code = "layout_mismatch"
                    last_initial_error = (
                        f"rendered Save bounds {list(box)} differ from required "
                        f"{list(EXPECTED_SAVE_BOX)} beyond {SAVE_LAYOUT_TOLERANCE}px")
                    time.sleep(.08)
                    continue
                ready = True
                break
            if not ready:
                raise VisualFailure(
                    "initial GUI state not reached: " + last_initial_error,
                    code=last_initial_code)
            center = ((box[0] + box[2]) // 2, (box[1] + box[3]) // 2)
            metadata["hover_mouse"] = mouse(
                disp, target, *center, expected_rect=expected_rect)
            time.sleep(.20)
            hovered = capture(disp, target, expected_rect=expected_rect,
                              desktop_path=out / "desktop_hover.png")
            hovered.save(out / "hover.png")
            metadata["save_click"] = mouse(
                disp, target, *center, click=True, expected_rect=expected_rect)
            metadata["real_mouse_event"] = True
            post, matched_since, matching_frames = None, None, 0
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise VisualFailure(
                        f"app exited before Save completed: {process.returncode}")
                frame = capture(disp, target, expected_rect=expected_rect,
                                desktop_path=out / "desktop_after.png")
                # Keep the latest frame even on a timeout or failed Save.
                frame.save(out / "after.png")
                changed = count_changed(hovered, frame, roi)
                metadata["changed_button_pixels"] = changed
                matched = changed >= args.minimum_changed
                if matched and args.ocr_after:
                    try:
                        metadata["after_ocr"] = button_ocr(
                            frame, box, out / "after_button_ocr.png", args.ocr_after)
                        metadata["after_ocr_observed"] = metadata["after_ocr"]
                    except OcrMismatch as exc:
                        metadata["after_ocr_last_mismatch"] = str(exc)
                        metadata["after_ocr_observed"] = exc.words
                        matched = False
                if matched:
                    matching_frames += 1
                    if matched_since is None:
                        matched_since = time.monotonic()
                    if matching_frames >= 2 and time.monotonic() - matched_since >= .22:
                        post = frame
                        break
                else:
                    matched_since, matching_frames = None, 0
                time.sleep(.08)
            if post is None:
                if args.ocr_after:
                    raise VisualFailure(
                        "real Save click did not reach stable visible completion: " +
                        metadata.get("after_ocr_last_mismatch", "no qualifying pixel change"),
                        code="completion_not_reached")
                raise VisualFailure(
                    "real Save click did not change visible button beyond hover",
                    code="completion_not_reached")
            post.save(out / "after.png")
            close_requested = True
            request_close(disp, target)
            try:
                code = process.wait(timeout=5)
            except subprocess.TimeoutExpired as exc:
                raise VisualFailure("GUI ignored real WM_DELETE_WINDOW") from exc
            if code != 0:
                raise VisualFailure(f"native GUI exited with code {code}")
            metadata.update(result="PASS", exit_code=code,
                            duration_seconds=round(time.monotonic() - start, 2))
    except BaseException as exc:
        metadata["error"] = str(exc)
        metadata["failure_code"] = getattr(exc, "code", type(exc).__name__)
        metadata["application_alive_on_failure"] = (
            process is not None and process.poll() is None)
        raise
    finally:
        try:
            # A visible mismatch still owns a live native application. Ask it to
            # close normally before any bounded forced cleanup; keep the original
            # visual failure and report cleanup separately for negative scenarios.
            if (process is not None and process.poll() is None and target is not None
                    and disp is not None and not close_requested):
                try:
                    request_close(disp, target)
                    process.wait(timeout=5)
                    metadata["cleanup_mode"] = "wm_delete_window"
                except Exception as exc:
                    metadata["cleanup_mode"] = "wm_delete_window_failed"
                    metadata["cleanup_error"] = str(exc)
            if process is not None and process.poll() is None:
                metadata["cleanup_mode"] = "terminate"
                process.terminate()
                try:
                    process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    metadata["cleanup_mode"] = "kill"
                    process.kill()
                    try:
                        process.wait(timeout=2)
                    except subprocess.TimeoutExpired:
                        metadata["cleanup_error"] = "native process did not exit after SIGKILL"
            if process is not None:
                metadata.setdefault("cleanup_mode",
                                    "wm_delete_window" if close_requested else "already_exited")
                metadata["cleanup_exit_code"] = process.poll()
        except Exception as exc:
            metadata["cleanup_error"] = str(exc)
        if disp is not None:
            try:
                disp.close()
            except Exception as exc:
                metadata["display_cleanup_error"] = str(exc)
                metadata.setdefault("cleanup_error", "X11 display close failed: " + str(exc))
        cleanup_failed = metadata["result"] == "PASS" and "cleanup_error" in metadata
        if cleanup_failed:
            metadata.update(result="FAIL", failure_code="cleanup_failed",
                            error="native GUI cleanup failed: " + metadata["cleanup_error"])
        metadata["duration_seconds"] = round(time.monotonic() - start, 2)
        (out / "evidence.json").write_text(
            json.dumps(metadata, indent=2, sort_keys=True) + "\n",
            encoding="utf-8")
        if cleanup_failed:
            raise VisualFailure(metadata["error"], code="cleanup_failed")
    print("GUI E2E PASS", metadata["source_sha"],
          "changed_pixels", metadata["changed_button_pixels"])
    return metadata


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", required=True)
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--timeout", type=float, default=12.0)
    parser.add_argument("--minimum-changed", type=int, default=38)
    parser.add_argument("--ocr-initial", default="")
    parser.add_argument("--ocr-after", default="")
    args = parser.parse_args(argv)
    try:
        run(args)
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print("GUI E2E FAIL:", error, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())

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
import os
from pathlib import Path
import re
import subprocess
import sys
import time

WIDTH, HEIGHT = 928, 608
TITLE = "FlexUI Desktop Editor"


class VisualFailure(RuntimeError):
    pass


class OcrMismatch(VisualFailure):
    """A readable frame has not reached the required visible state yet."""


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
    return int(pos.x), int(pos.y), int(geometry.width), int(geometry.height)


def capture(disp, window):
    from PIL import ImageGrab
    x, y, w, h = window_rect(disp, window)
    screen = ImageGrab.grab(xdisplay=os.environ["DISPLAY"])
    if x < 0 or y < 0 or x + w > screen.width or y + h > screen.height:
        raise VisualFailure("the real GUI is not fully visible on its X11 screen")
    return screen.crop((x, y, x + w, y + h)).convert("RGB")


def mouse(disp, window, x, y, *, click=False):
    from Xlib import X
    from Xlib.ext import xtest
    ox, oy, w, h = window_rect(disp, window)
    if not (0 <= x < w and 0 <= y < h):
        raise VisualFailure("mouse target was outside real window")
    disp.set_input_focus(window, X.RevertToParent, X.CurrentTime)
    xtest.fake_input(disp, X.MotionNotify, x=ox + x, y=oy + y)
    disp.sync()
    if click:
        xtest.fake_input(disp, X.ButtonPress, 1)
        disp.sync()
        time.sleep(.06)
        xtest.fake_input(disp, X.ButtonRelease, 1)
        disp.sync()


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
            continue
        if conf >= 20 and fields[11].strip():
            words.append({
                "text": fields[11].strip(), "confidence": conf,
                "box": [int(fields[i]) for i in (6, 7, 8, 9)]
            })
    found = re.findall(r"\w+", " ".join(x["text"] for x in words).lower())
    wanted = re.findall(r"\w+", expected.lower())
    # Compare whole words: "Unsaved" must never satisfy the "Saved" gate.
    if not wanted or not any(found[i:i + len(wanted)] == wanted
                             for i in range(len(found) - len(wanted) + 1)):
        raise OcrMismatch(f"visible OCR {found!r} did not contain words {wanted!r}")
    return words


def button_ocr(frame, roi, path, expected):
    from PIL import ImageOps
    box = (max(0, roi[0] - 12), max(0, roi[1] - 9),
           min(frame.width, roi[2] + 30), min(frame.height, roi[3] + 10))
    crop = frame.crop(box)
    crop = ImageOps.expand(crop.resize((crop.width * 3, crop.height * 3)),
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
    disp, process = None, None
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
            # GLFW maps the window before XML/controller initialization has
            # necessarily rendered. Wait for actual pixels, not a fixed sleep.
            ready_deadline = min(deadline, time.monotonic() + 6.0)
            ready, last_initial_error = False, "no frame captured"
            initial_samples, initial_stddev = 0, 0.0
            while time.monotonic() < ready_deadline:
                if process.poll() is not None:
                    raise VisualFailure(
                        f"app exited before rendered Save: {process.returncode}")
                before = capture(disp, target)
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
                if args.ocr_initial:
                    try:
                        metadata["initial_ocr"] = button_ocr(
                            before, roi, out / "before_button_ocr.png",
                            args.ocr_initial)
                    except OcrMismatch as exc:
                        last_initial_error = str(exc)
                        metadata["initial_ocr_last_mismatch"] = str(exc)
                        time.sleep(.08)
                        continue
                ready = True
                break
            if not ready:
                raise VisualFailure("initial GUI state not reached: " + last_initial_error)
            metadata["save_button_box"] = list(box)
            center = ((box[0] + box[2]) // 2, (box[1] + box[3]) // 2)
            mouse(disp, target, *center)
            time.sleep(.20)
            hovered = capture(disp, target)
            hovered.save(out / "hover.png")
            mouse(disp, target, *center, click=True)
            metadata["real_mouse_event"] = True
            post, matched_since, matching_frames = None, None, 0
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise VisualFailure(
                        f"app exited before Save completed: {process.returncode}")
                frame = capture(disp, target)
                # Keep the latest frame even on a timeout or failed Save.
                frame.save(out / "after.png")
                changed = count_changed(hovered, frame, roi)
                metadata["changed_button_pixels"] = changed
                matched = changed >= args.minimum_changed
                if matched and args.ocr_after:
                    try:
                        metadata["after_ocr"] = button_ocr(
                            frame, roi, out / "after_button_ocr.png", args.ocr_after)
                    except OcrMismatch as exc:
                        metadata["after_ocr_last_mismatch"] = str(exc)
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
                        metadata.get("after_ocr_last_mismatch", "no qualifying pixel change"))
                raise VisualFailure(
                    "real Save click did not change visible button beyond hover")
            post.save(out / "after.png")
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
        raise
    finally:
        if process is not None and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        if disp is not None:
            disp.close()
        (out / "evidence.json").write_text(
            json.dumps(metadata, indent=2, sort_keys=True) + "\n",
            encoding="utf-8")
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

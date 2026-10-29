#!/usr/bin/env python3
"""Qualify the real Desktop Editor and require four visible application failures.

All five cases use the same native executable and document-service plugin.
Only isolated copies of XML/CSS/TurboScript assets receive the deliberate defects.
"""
from __future__ import annotations

import argparse
from contextlib import redirect_stderr, redirect_stdout
import json
import math
from pathlib import Path
import re
import shutil
import sys
import tempfile
import time
from types import SimpleNamespace

import gui_native_e2e as gui


ASSETS = ("editor.xml", "editor.css", "editor.tbs")
PLUGIN = "libflexui_document_service.so"
INITIAL_LABEL = "Save"
# The real service response starts with these words; the rest may be ellipsized.
COMPLETION_LABEL = "Saved by"
SCENARIOS = (
    {"name": "negative-pending", "asset": "editor.tbs",
     "old": "text: value.payload", "new": 'text: "Saving..."',
     "failure_code": "completion_not_reached", "observed_label": "Saving",
     "observation": "after_ocr_observed", "forbidden_label": "Saved", "mouse": True},
    {"name": "negative-initial-label", "asset": "editor.xml",
     "old": ">Save</button>", "new": ">Broken</button>",
     "failure_code": "initial_state_not_reached", "observed_label": "Broken",
     "observation": "initial_ocr_observed", "forbidden_label": "Save", "mouse": False},
    {"name": "negative-layout", "asset": "editor.css",
     "old": "width: 680px;", "new": "width: 600px;",
     "failure_code": "layout_mismatch", "observed_label": "Save",
     "observation": "initial_ocr_observed", "forbidden_label": "Saved", "mouse": False,
     "layout_offset": [-80, 0, -80, 0]},
    {"name": "negative-not-saved", "asset": "editor.tbs",
     "old": "text: value.payload", "new": 'text: "NOT SAVED"',
     "failure_code": "completion_not_reached", "observed_label": "NOT SAVED",
     "observation": "after_ocr_observed", "forbidden_label": COMPLETION_LABEL, "mouse": True},
)


class SuiteFailure(RuntimeError):
    pass


def require(condition, message):
    if not condition:
        raise SuiteFailure(message)


def contains_phrase(words, expected):
    """Find conflicting confident words anywhere in a negative observation."""
    if not isinstance(words, list):
        return False
    tokens = []
    for word in words:
        if not isinstance(word, dict) or not isinstance(word.get("text"), str):
            return False
        try:
            confidence = float(word.get("confidence", -1))
        except (TypeError, ValueError):
            confidence = -1
        confident = math.isfinite(confidence) and 20 <= confidence <= 100
        tokens.extend((token, confident) for token in re.findall(r"\w+", word["text"].lower()))
    wanted = re.findall(r"\w+", expected.lower())
    return bool(wanted) and any(
        all(confident and token == expected_token
            for (token, confident), expected_token in zip(tokens[start:start + len(wanted)], wanted))
        for start in range(len(tokens) - len(wanted) + 1))


def runtime_identity(exe):
    root = exe.parent
    paths = {"binary": exe, "plugin": root / PLUGIN}
    paths.update({name: root / "desktop_editor_assets" / name for name in ASSETS})
    for name, path in paths.items():
        require(path.is_file(), f"missing real runtime {name}: {path}")
    return {name: gui.sha256_file(path) for name, path in paths.items()}


def integer_coordinates(value, length):
    return isinstance(value, list) and len(value) == length and all(
        type(coordinate) is int for coordinate in value)


def validate_window_placement(evidence):
    rect = evidence.get("window_rect")
    require(integer_coordinates(rect, 4) and
            abs(rect[2] - gui.WIDTH) <= 8 and abs(rect[3] - gui.HEIGHT) <= 8,
            "wrong native window geometry")
    require(rect[:2] == list(gui.WINDOW_ORIGIN),
            "native window did not reach the required nonzero screen origin")
    placement = evidence.get("window_placement")
    require(isinstance(placement, dict), "missing native window placement evidence")
    initial = placement.get("initial_rect")
    require(integer_coordinates(initial, 4) and initial[2:] == rect[2:],
            "window placement did not preserve the initial client dimensions")
    requested = placement.get("requested_origin")
    require(integer_coordinates(requested, 2) and requested == list(gui.WINDOW_ORIGIN),
            "window placement requested the wrong screen origin")
    observed = placement.get("observed_rect")
    require(integer_coordinates(observed, 4) and observed == rect,
            "window placement disagrees with the observed native client rectangle")
    screen = placement.get("screen_size")
    require(integer_coordinates(screen, 2) and screen[0] > 0 and screen[1] > 0 and
            0 <= rect[0] < rect[0] + rect[2] <= screen[0] and
            0 <= rect[1] < rect[1] + rect[3] <= screen[1],
            "placed native client rectangle is not fully visible on its desktop")
    return placement


def validate_mouse_coordinates(evidence, clicked):
    if not clicked:
        require("hover_mouse" not in evidence and "save_click" not in evidence,
                "pre-click negative unexpectedly contains native mouse actions")
        return
    rect, box = evidence["window_rect"], evidence["save_button_box"]
    center = [(box[0] + box[2]) // 2, (box[1] + box[3]) // 2]
    screen = [rect[0] + center[0], rect[1] + center[1]]
    for key, action in (("hover_mouse", "move"), ("save_click", "left_click")):
        mouse = evidence.get(key)
        require(isinstance(mouse, dict) and mouse.get("action") == action,
                f"missing or wrong native {key} action")
        require(integer_coordinates(mouse.get("window_rect"), 4) and
                mouse["window_rect"] == rect,
                f"native {key} used a different client rectangle")
        require(integer_coordinates(mouse.get("client"), 2) and mouse["client"] == center,
                f"native {key} did not target the rendered Save center")
        require(integer_coordinates(mouse.get("screen"), 2) and mouse["screen"] == screen,
                f"native {key} did not translate client coordinates to the screen")
        require(integer_coordinates(mouse.get("observed_screen"), 2) and
                mouse["observed_screen"] == screen and
                integer_coordinates(mouse.get("observed_client"), 2) and
                mouse["observed_client"] == center,
                f"native {key} pointer observation disagrees with the requested target")


def validate_common(evidence, identity, source_sha):
    require(evidence.get("source_sha") == source_sha, "wrong source HEAD in GUI evidence")
    require(evidence.get("binary_sha256") == identity["binary"],
            "wrong native binary in GUI evidence")
    require(evidence.get("visible_window") is True, "real application window was not visible")
    validate_window_placement(evidence)
    rect = evidence["window_rect"]
    box = evidence.get("save_button_box")
    require(isinstance(box, list) and len(box) == 4 and
            all(type(value) is int for value in box), "rendered button was not identified")
    require(0 <= box[0] < box[2] <= rect[2] and 0 <= box[1] < box[3] <= rect[3],
            "rendered button geometry is outside the native window")
    layout = gui.button_layout(box)
    require(evidence.get("save_button_layout") == layout,
            "layout diagnosis does not match the observed button geometry")
    return layout


def validate_positive(evidence, identity, source_sha, minimum_changed):
    layout = validate_common(evidence, identity, source_sha)
    require(evidence.get("result") == "PASS" and layout["matches"] is True,
            "normal GUI did not satisfy the expected button layout")
    require(evidence.get("real_mouse_event") is True and evidence.get("exit_code") == 0 and
            evidence.get("changed_button_pixels", 0) >= minimum_changed and
            gui.label_starts_with(evidence.get("initial_ocr"), INITIAL_LABEL) and
            gui.label_starts_with(evidence.get("after_ocr"), COMPLETION_LABEL),
            "normal GUI lacks complete Save-to-Saved evidence")
    validate_mouse_coordinates(evidence, True)
    require(evidence.get("cleanup_mode") == "wm_delete_window" and
            evidence.get("cleanup_exit_code") == 0 and "cleanup_error" not in evidence,
            "normal GUI did not close cleanly through its native window")


def validate_negative(evidence, scenario, identity, source_sha, minimum_changed):
    layout = validate_common(evidence, identity, source_sha)
    offset = scenario.get("layout_offset")
    if offset is None:
        require(layout["matches"] is True, "text negative scenario has an unrelated layout defect")
    else:
        require(layout["matches"] is False, "deliberately shifted button kept the normal layout")
        require(all(abs(actual - expected) <= gui.SAVE_LAYOUT_TOLERANCE
                    for actual, expected in zip(layout["edge_deltas"], offset)),
                "layout negative did not move the button by the expected offset")
        box, expected = layout["observed_box"], layout["expected_box"]
        require(abs((box[2] - box[0]) - (expected[2] - expected[0])) <=
                gui.SAVE_LAYOUT_TOLERANCE and
                abs((box[3] - box[1]) - (expected[3] - expected[1])) <=
                gui.SAVE_LAYOUT_TOLERANCE,
                "layout negative unexpectedly changed the button size")
        require(gui.label_starts_with(evidence.get("initial_ocr"), INITIAL_LABEL),
                "layout negative did not render the unchanged Save label")
    require(evidence.get("result") == "FAIL", "deliberately broken GUI was accepted")
    require(evidence.get("failure_code") == scenario["failure_code"],
            "GUI failed in the wrong phase")
    require(evidence.get("application_alive_on_failure") is True,
            "application startup failure or crash is not an expected visual failure")
    require(evidence.get("real_mouse_event") is scenario["mouse"],
            "wrong real mouse evidence for the negative scenario")
    validate_mouse_coordinates(evidence, scenario["mouse"])
    require(evidence.get("cleanup_mode") == "wm_delete_window" and
            evidence.get("cleanup_exit_code") == 0 and "cleanup_error" not in evidence,
            "broken GUI did not close cleanly through its native window")
    observed = evidence.get(scenario["observation"])
    require(gui.label_starts_with(observed, scenario["observed_label"]),
            "the expected visible negative-state label was not actually recognized")
    require(not contains_phrase(observed, scenario["forbidden_label"]),
            "negative OCR observation also claims the required success state")
    if scenario["mouse"]:
        require(gui.label_starts_with(evidence.get("initial_ocr"), INITIAL_LABEL),
                "completion negative did not start with visible Save")
        require(evidence.get("changed_button_pixels", 0) >= minimum_changed,
                "completion negative did not produce a real post-click pixel change")


def verify_desktop_screenshots(directory, evidence, clicked):
    """Require client pixels to come from the retained off-origin root capture."""
    from PIL import Image
    placement = validate_window_placement(evidence)
    x, y, width, height = evidence["window_rect"]
    bounds = (x, y, x + width, y + height)
    records = []
    for name in (("before", "hover", "after") if clicked else ("before",)):
        client_path, desktop_path = directory / f"{name}.png", directory / f"desktop_{name}.png"
        require(client_path.is_file() and desktop_path.is_file(),
                f"missing retained {name} client or desktop screenshot")
        with Image.open(client_path) as client, Image.open(desktop_path) as desktop:
            require(client.mode == "RGB" and desktop.mode == "RGB",
                    f"retained {name} screenshots must preserve RGB pixels")
            require(client.size == (width, height),
                    f"retained {name} client screenshot has wrong dimensions")
            require(list(desktop.size) == placement["screen_size"],
                    f"retained {name} desktop screenshot has wrong dimensions")
            require(desktop.crop(bounds).tobytes() == client.tobytes(),
                    f"retained {name} client pixels do not match the desktop at the placed origin")
        records.append({"frame": name, "desktop": desktop_path.name,
                        "client": client_path.name, "crop_rect": list(bounds),
                        "pixels_match": True})
    return records


def verify_negative_screenshots(directory, evidence, scenario, minimum_changed):
    from PIL import Image
    with Image.open(directory / "before.png") as image:
        before = image.convert("RGB")
    require(list(gui.detect_button(before)) == evidence["save_button_box"],
            "negative screenshot does not contain the observed button")
    box = evidence["save_button_box"]
    require(gui.button_layout(box) == evidence["save_button_layout"],
            "retained screenshot disagrees with the layout diagnosis")
    roi = (max(0, box[0] - 5), max(0, box[1] - 3),
           min(before.width, box[2] + 5), min(before.height, box[3] + 3))
    observed_frame = before
    if scenario["mouse"]:
        with Image.open(directory / "hover.png") as image:
            hover = image.convert("RGB")
        with Image.open(directory / "after.png") as image:
            after = image.convert("RGB")
        require(gui.count_changed(hover, after, roi) >= minimum_changed,
                "retained completion-negative frame has no qualifying real pixel change")
        observed_frame = after
    # Recompute the crop from retained native screenshots, not just a failure
    # string, JSON word list or potentially stale standalone OCR crop.
    return gui.button_ocr(observed_frame, box, directory / "verified_visible_label_ocr.png",
                          scenario["observed_label"])


def execute_driver(args, exe, directory):
    directory.mkdir(parents=True, exist_ok=True)
    driver_args = SimpleNamespace(
        exe=str(exe), source_sha=args.source_sha, out=str(directory),
        timeout=args.timeout, minimum_changed=args.minimum_changed,
        ocr_initial=INITIAL_LABEL, ocr_after=COMPLETION_LABEL)
    rejected = False
    with (directory / "driver.log").open("w", encoding="utf-8") as log:
        with redirect_stdout(log), redirect_stderr(log):
            try:
                gui.run(driver_args)
            except gui.VisualFailure as exc:
                rejected = True
                print(f"GUI E2E FAIL: {exc}")
    evidence = json.loads((directory / "evidence.json").read_text(encoding="utf-8"))
    return evidence, rejected


def prepare_negative(exe, root, scenario, identity):
    root.mkdir()
    copied = root / exe.name
    shutil.copy2(exe, copied)
    shutil.copy2(exe.parent / PLUGIN, root / PLUGIN)
    assets = root / "desktop_editor_assets"
    assets.mkdir()
    for name in ASSETS:
        shutil.copy2(exe.parent / "desktop_editor_assets" / name, assets / name)
    require(runtime_identity(copied) == identity, "runtime copy differs from the real built runtime")
    path = assets / scenario["asset"]
    original, old, new = path.read_bytes(), scenario["old"].encode(), scenario["new"].encode()
    require(original.count(old) == 1, "negative mutation must match exactly one asset location")
    path.write_bytes(original.replace(old, new, 1))
    mutated = runtime_identity(copied)
    changed = [name for name in identity if mutated[name] != identity[name]]
    require(changed == [scenario["asset"]], "negative scenario changed more than its intended asset")
    return copied, mutated


def run_suite(args):
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    summary = {"result": "FAIL", "source_sha": args.source_sha, "negative_cases": []}
    identity, exe = None, None
    start = time.monotonic()
    try:
        require(re.fullmatch(r"[0-9a-f]{40}", args.source_sha) is not None,
                "exact 40-character source HEAD required")
        require(math.isfinite(args.timeout) and 0 < args.timeout <= 60,
                "per-case timeout must be finite and between 0 and 60 seconds")
        require(args.minimum_changed >= 1, "minimum changed pixels must be positive")
        exe = Path(args.exe).resolve(strict=True)
        identity = runtime_identity(exe)
        summary["original_runtime_sha256"] = identity
        positive, rejected = execute_driver(args, exe, out)
        summary["positive_window_placement"] = positive.get("window_placement")
        summary["positive_hover_mouse"] = positive.get("hover_mouse")
        summary["positive_save_click"] = positive.get("save_click")
        require(not rejected and positive.get("result") == "PASS", "normal real GUI did not pass")
        validate_positive(positive, identity, args.source_sha, args.minimum_changed)
        summary["positive_desktop_screenshots"] = verify_desktop_screenshots(out, positive, True)
        summary["positive_result"] = "PASS"
        summary["positive_button_layout"] = positive["save_button_layout"]
        require(runtime_identity(exe) == identity, "positive run changed original runtime files")
        with tempfile.TemporaryDirectory(prefix="stun-gui-negative-") as temporary:
            for scenario in SCENARIOS:
                record = {"name": scenario["name"], "expected_failure_observed": False,
                          "mutation": {key: scenario[key] for key in ("asset", "old", "new")}}
                summary["negative_cases"].append(record)
                copied, mutated = prepare_negative(exe, Path(temporary) / scenario["name"],
                                                   scenario, identity)
                record["runtime_sha256"] = mutated
                evidence_dir = out / scenario["name"]
                evidence, rejected = execute_driver(args, copied, evidence_dir)
                record["driver_result"] = evidence.get("result")
                record["failure_code"] = evidence.get("failure_code")
                record["error"] = evidence.get("error")
                record["save_button_layout"] = evidence.get("save_button_layout")
                record["window_placement"] = evidence.get("window_placement")
                for key in ("hover_mouse", "save_click"):
                    if key in evidence:
                        record[key] = evidence[key]
                require(rejected, "deliberately broken GUI did not raise a visual failure")
                validate_negative(evidence, scenario, identity, args.source_sha,
                                  args.minimum_changed)
                record["desktop_screenshots"] = verify_desktop_screenshots(
                    evidence_dir, evidence, scenario["mouse"])
                record["verified_visible_label_ocr"] = verify_negative_screenshots(
                    evidence_dir, evidence, scenario, args.minimum_changed)
                require(runtime_identity(copied) == mutated, "negative run changed its runtime files")
                require(runtime_identity(exe) == identity, "negative run changed original runtime files")
                record["expected_failure_observed"] = True
        summary["result"] = "PASS"
    except BaseException as exc:
        summary["error"] = str(exc)
        raise
    finally:
        integrity_error = None
        if identity is not None:
            try:
                require(runtime_identity(exe) == identity, "original runtime changed during GUI suite")
                summary["original_runtime_unchanged"] = True
            except Exception as exc:
                integrity_error = exc
                summary.update(result="FAIL", original_runtime_unchanged=False,
                               integrity_error=str(exc))
        summary["duration_seconds"] = round(time.monotonic() - start, 2)
        (out / "suite.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n",
                                        encoding="utf-8")
        if integrity_error is not None:
            raise integrity_error
    print("GUI SUITE PASS", args.source_sha,
          f"normal GUI and {len(SCENARIOS)} expected visible failures")
    return summary


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", required=True)
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--timeout", type=float, default=12.0)
    parser.add_argument("--minimum-changed", type=int, default=38)
    args = parser.parse_args(argv)
    try:
        run_suite(args)
        return 0
    except Exception as exc:
        print("GUI SUITE FAIL:", exc, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())

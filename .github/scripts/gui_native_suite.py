#!/usr/bin/env python3
"""Qualify the real Desktop Editor and require two visible application failures.

All three cases use the same native executable and document-service plugin.
Only isolated copies of XML/TurboScript assets receive the deliberate defects.
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
SCENARIOS = (
    {"name": "negative-pending", "asset": "editor.tbs",
     "old": "text: value.payload", "new": 'text: "Saving..."',
     "failure_code": "completion_not_reached", "observed_word": "Saving",
     "observation": "after_ocr_observed", "mouse": True},
    {"name": "negative-initial-label", "asset": "editor.xml",
     "old": ">Save</button>", "new": ">Broken</button>",
     "failure_code": "initial_state_not_reached", "observed_word": "Broken",
     "observation": "initial_ocr_observed", "mouse": False},
)


class SuiteFailure(RuntimeError):
    pass


def require(condition, message):
    if not condition:
        raise SuiteFailure(message)


def contains_word(words, expected):
    """Require a whole OCR word with the driver's confidence threshold."""
    if not isinstance(words, list):
        return False
    for word in words:
        if not isinstance(word, dict) or not isinstance(word.get("text"), str):
            continue
        try:
            confidence = float(word.get("confidence", -1))
        except (TypeError, ValueError):
            continue
        if math.isfinite(confidence) and 20 <= confidence <= 100:
            if expected.lower() in re.findall(r"\w+", word["text"].lower()):
                return True
    return False


def runtime_identity(exe):
    root = exe.parent
    paths = {"binary": exe, "plugin": root / PLUGIN}
    paths.update({name: root / "desktop_editor_assets" / name for name in ASSETS})
    for name, path in paths.items():
        require(path.is_file(), f"missing real runtime {name}: {path}")
    return {name: gui.sha256_file(path) for name, path in paths.items()}


def validate_common(evidence, identity, source_sha):
    require(evidence.get("source_sha") == source_sha, "wrong source HEAD in GUI evidence")
    require(evidence.get("binary_sha256") == identity["binary"],
            "wrong native binary in GUI evidence")
    require(evidence.get("visible_window") is True, "real application window was not visible")
    rect = evidence.get("window_rect", [])
    require(isinstance(rect, list) and len(rect) == 4 and
            abs(rect[2] - gui.WIDTH) <= 8 and abs(rect[3] - gui.HEIGHT) <= 8,
            "wrong native window geometry")
    require(isinstance(evidence.get("save_button_box"), list) and
            len(evidence["save_button_box"]) == 4, "rendered button was not identified")


def validate_negative(evidence, scenario, identity, source_sha, minimum_changed):
    validate_common(evidence, identity, source_sha)
    require(evidence.get("result") == "FAIL", "deliberately broken GUI was accepted")
    require(evidence.get("failure_code") == scenario["failure_code"],
            "GUI failed in the wrong phase")
    require(evidence.get("application_alive_on_failure") is True,
            "application startup failure or crash is not an expected visual failure")
    require(evidence.get("real_mouse_event") is scenario["mouse"],
            "wrong real mouse evidence for the negative scenario")
    require(evidence.get("cleanup_mode") == "wm_delete_window" and
            evidence.get("cleanup_exit_code") == 0 and "cleanup_error" not in evidence,
            "broken GUI did not close cleanly through its native window")
    observed = evidence.get(scenario["observation"])
    require(contains_word(observed, scenario["observed_word"]),
            "the deliberately wrong label was not actually recognized")
    require(not contains_word(observed, "Saved" if scenario["mouse"] else "Save"),
            "negative OCR observation also claims the required success state")
    if scenario["mouse"]:
        require(contains_word(evidence.get("initial_ocr"), "Save"),
                "pending scenario did not start with visible Save")
        require(evidence.get("changed_button_pixels", 0) >= minimum_changed,
                "pending scenario did not produce a real post-click pixel change")


def verify_negative_screenshots(directory, evidence, scenario, minimum_changed):
    from PIL import Image
    with Image.open(directory / "before.png") as image:
        before = image.convert("RGB")
    require(list(gui.detect_button(before)) == evidence["save_button_box"],
            "negative screenshot does not contain the observed button")
    box = evidence["save_button_box"]
    roi = (max(0, box[0] - 5), max(0, box[1] - 3),
           min(before.width, box[2] + 5), min(before.height, box[3] + 3))
    observed_frame = before
    if scenario["mouse"]:
        with Image.open(directory / "hover.png") as image:
            hover = image.convert("RGB")
        with Image.open(directory / "after.png") as image:
            after = image.convert("RGB")
        require(gui.count_changed(hover, after, roi) >= minimum_changed,
                "retained pending frame has no qualifying real pixel change")
        observed_frame = after
    # Recompute the crop from retained native screenshots, not just a failure
    # string, JSON word list or potentially stale standalone OCR crop.
    return gui.button_ocr(observed_frame, roi, directory / "verified_wrong_label_ocr.png",
                          scenario["observed_word"])


def execute_driver(args, exe, directory):
    directory.mkdir(parents=True, exist_ok=True)
    driver_args = SimpleNamespace(
        exe=str(exe), source_sha=args.source_sha, out=str(directory),
        timeout=args.timeout, minimum_changed=args.minimum_changed,
        ocr_initial="Save", ocr_after="Saved")
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
        require(not rejected and positive.get("result") == "PASS", "normal real GUI did not pass")
        validate_common(positive, identity, args.source_sha)
        require(positive.get("real_mouse_event") is True and positive.get("exit_code") == 0 and
                positive.get("changed_button_pixels", 0) >= args.minimum_changed and
                contains_word(positive.get("initial_ocr"), "Save") and
                contains_word(positive.get("after_ocr"), "Saved"),
                "normal GUI lacks complete Save-to-Saved evidence")
        summary["positive_result"] = "PASS"
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
                require(rejected, "deliberately broken GUI did not raise a visual failure")
                validate_negative(evidence, scenario, identity, args.source_sha,
                                  args.minimum_changed)
                record["verified_wrong_label_ocr"] = verify_negative_screenshots(
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
    print("GUI SUITE PASS", args.source_sha, "normal GUI and two expected visible failures")
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

#!/usr/bin/env python3
"""Admission/isolation regressions, not a native Stun GUI qualification."""
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import gui_native_suite as suite


SHA = "a" * 40
IDENTITY = {"binary": "b" * 64, "plugin": "c" * 64}


def words(text):
    return [{"text": part, "confidence": 95.0, "box": [10, 10, 30, 12]}
            for part in text.split()]


def negative_evidence(scenario):
    return {
        "source_sha": SHA, "binary_sha256": IDENTITY["binary"], "result": "FAIL",
        "visible_window": True, "window_rect": [0, 0, 928, 608],
        "save_button_box": [721, 25, 855, 59],
        "failure_code": scenario["failure_code"], "application_alive_on_failure": True,
        "real_mouse_event": scenario["mouse"], "cleanup_mode": "wm_delete_window",
        "cleanup_exit_code": 0, "changed_button_pixels": 996,
        "initial_ocr": words("Save") if scenario["mouse"] else [],
        scenario["observation"]: words(scenario["observed_word"]),
    }


class NegativeAdmissionTests(unittest.TestCase):
    def validate(self, evidence, scenario):
        suite.validate_negative(evidence, scenario, IDENTITY, SHA, 38)

    def test_both_precisely_observed_failures_are_accepted(self):
        for scenario in suite.SCENARIOS:
            with self.subTest(scenario=scenario["name"]):
                self.validate(negative_evidence(scenario), scenario)

    def test_startup_crash_identity_and_wrong_phase_are_rejected(self):
        for scenario in suite.SCENARIOS:
            changes = (
                {"result": "PASS"},
                {"failure_code": "gui_failure"},
                {"failure_code": "completion_not_reached" if not scenario["mouse"]
                                 else "initial_state_not_reached"},
                {"visible_window": False},
                {"application_alive_on_failure": False},
                {"source_sha": "d" * 40},
                {"binary_sha256": "e" * 64},
                {"real_mouse_event": not scenario["mouse"]},
                {"window_rect": [0, 0, 320, 180]},
                {"save_button_box": None},
                {"cleanup_mode": "terminate"},
                {"cleanup_exit_code": 1},
                {"cleanup_error": "X11 BadWindow"},
            )
            for change in changes:
                with self.subTest(scenario=scenario["name"], change=change):
                    evidence = negative_evidence(scenario)
                    evidence.update(change)
                    with self.assertRaises(suite.SuiteFailure):
                        self.validate(evidence, scenario)

    def test_empty_uncertain_and_wrong_ocr_are_rejected(self):
        for scenario in suite.SCENARIOS:
            uncertain = words(scenario["observed_word"])
            uncertain[0]["confidence"] = 19.9
            nonfinite = words(scenario["observed_word"])
            nonfinite[0]["confidence"] = float("nan")
            for observed in (None, [], words("error"), words("Unsaved"), uncertain, nonfinite):
                with self.subTest(scenario=scenario["name"], observed=observed):
                    evidence = negative_evidence(scenario)
                    evidence[scenario["observation"]] = observed
                    with self.assertRaises(suite.SuiteFailure):
                        self.validate(evidence, scenario)

    def test_pending_requires_initial_save_and_real_pixel_change(self):
        scenario = suite.SCENARIOS[0]
        for change in ({"initial_ocr": []}, {"initial_ocr": words("Saving")},
                       {"changed_button_pixels": 0}, {"changed_button_pixels": 37}):
            with self.subTest(change=change):
                evidence = negative_evidence(scenario)
                evidence.update(change)
                with self.assertRaises(suite.SuiteFailure):
                    self.validate(evidence, scenario)

    def test_conflicting_success_observation_is_not_expected_failure(self):
        for scenario in suite.SCENARIOS:
            evidence = negative_evidence(scenario)
            evidence[scenario["observation"]] += words("Saved" if scenario["mouse"] else "Save")
            with self.subTest(scenario=scenario["name"]), self.assertRaises(suite.SuiteFailure):
                self.validate(evidence, scenario)

    def test_nonvisual_exception_is_not_swallowed(self):
        args = SimpleNamespace(source_sha=SHA, timeout=12, minimum_changed=38)
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(suite.gui, "run", side_effect=OSError("missing dependency")):
                with self.assertRaisesRegex(OSError, "missing dependency"):
                    suite.execute_driver(args, Path("/unused/native-gui"), Path(directory))


class IsolationTests(unittest.TestCase):
    def create_files(self, root):
        root.mkdir()
        exe = root / "flexui_desktop_editor"
        exe.write_bytes(b"unit-test identity bytes, never executed as a GUI")
        (root / suite.PLUGIN).write_bytes(b"unit-test plugin identity bytes")
        assets = root / "desktop_editor_assets"
        assets.mkdir()
        (assets / "editor.xml").write_text('<button id="save">Save</button>\n', encoding="utf-8")
        (assets / "editor.tbs").write_text('map {text: value.payload}\n', encoding="utf-8")
        (assets / "editor.css").write_text('#save { color: white; }\n', encoding="utf-8")
        return exe

    def test_only_target_asset_changes_and_runtime_is_copied(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            exe = self.create_files(root / "original")
            identity = suite.runtime_identity(exe)
            for scenario in suite.SCENARIOS:
                with self.subTest(scenario=scenario["name"]):
                    copied, mutated = suite.prepare_negative(
                        exe, root / scenario["name"], scenario, identity)
                    self.assertFalse(copied.is_symlink())
                    self.assertEqual(copied.read_bytes(), exe.read_bytes())
                    self.assertEqual(suite.runtime_identity(exe), identity)
                    self.assertEqual([name for name in identity if identity[name] != mutated[name]],
                                     [scenario["asset"]])
                    target = copied.parent / "desktop_editor_assets" / scenario["asset"]
                    self.assertIn(scenario["new"], target.read_text(encoding="utf-8"))

    def test_nonunique_mutation_is_configuration_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            exe = self.create_files(root / "original")
            asset = exe.parent / "desktop_editor_assets" / "editor.tbs"
            asset.write_text('text: value.payload\ntext: value.payload\n', encoding="utf-8")
            with self.assertRaisesRegex(suite.SuiteFailure, "exactly one"):
                suite.prepare_negative(exe, root / "negative", suite.SCENARIOS[0],
                                       suite.runtime_identity(exe))

    def test_missing_binary_writes_failed_suite_summary(self):
        with tempfile.TemporaryDirectory() as directory:
            args = SimpleNamespace(exe=str(Path(directory) / "missing"), source_sha=SHA,
                                   out=directory, timeout=12, minimum_changed=38)
            with self.assertRaises(FileNotFoundError):
                suite.run_suite(args)
            summary = json.loads((Path(directory) / "suite.json").read_text(encoding="utf-8"))
            self.assertEqual(summary["result"], "FAIL")
            self.assertEqual(summary["negative_cases"], [])
            self.assertIn("error", summary)


if __name__ == "__main__":
    unittest.main(verbosity=1)

#!/usr/bin/env python3
"""Admission/isolation regressions, not a native Stun GUI qualification."""
from copy import deepcopy
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import gui_native_suite as suite

try:
    from PIL import Image, ImageDraw
except ImportError:
    Image = ImageDraw = None


SHA = "a" * 40
IDENTITY = {"binary": "b" * 64, "plugin": "c" * 64}
WINDOW_RECT = [127, 83, 928, 608]
SCREEN_SIZE = [1280, 900]


def words(text):
    return [{"text": part, "confidence": 95.0, "box": [10, 10, 30, 12]}
            for part in text.split()]


def mouse_evidence(action, box):
    client = [(box[0] + box[2]) // 2, (box[1] + box[3]) // 2]
    screen = [127 + client[0], 83 + client[1]]
    return {"action": action, "window_rect": list(WINDOW_RECT),
            "client": client, "screen": screen,
            "observed_screen": list(screen), "observed_client": list(client)}


def negative_evidence(scenario):
    box = [721, 25, 855, 59]
    if "layout_offset" in scenario:
        box = [edge + offset for edge, offset in zip(box, scenario["layout_offset"])]
    evidence = {
        "source_sha": SHA, "binary_sha256": IDENTITY["binary"], "result": "FAIL",
        "visible_window": True, "window_rect": list(WINDOW_RECT),
        "window_placement": {"initial_rect": [0, 0, 928, 608],
                             "requested_origin": [127, 83],
                             "observed_rect": list(WINDOW_RECT),
                             "screen_size": list(SCREEN_SIZE)},
        "save_button_box": box, "save_button_layout": suite.gui.button_layout(box),
        "failure_code": scenario["failure_code"], "application_alive_on_failure": True,
        "real_mouse_event": scenario["mouse"], "cleanup_mode": "wm_delete_window",
        "cleanup_exit_code": 0, "changed_button_pixels": 996,
        "initial_ocr": words("Save") if scenario["mouse"] or "layout_offset" in scenario else [],
        scenario["observation"]: words(scenario["observed_label"]),
    }
    if scenario["mouse"]:
        evidence["hover_mouse"] = mouse_evidence("move", box)
        evidence["save_click"] = mouse_evidence("left_click", box)
    return evidence


def positive_evidence():
    evidence = negative_evidence(suite.SCENARIOS[0])
    evidence.pop("failure_code")
    evidence.pop("application_alive_on_failure")
    evidence.update(result="PASS", exit_code=0, after_ocr=words("Saved by native..."),
                    after_ocr_observed=words("Saved by native..."))
    return evidence


class PositiveAdmissionTests(unittest.TestCase):
    def test_normal_geometry_with_actual_save_evidence_is_accepted(self):
        suite.validate_positive(positive_evidence(), IDENTITY, SHA, 38)

    def test_negated_or_incomplete_service_labels_are_rejected(self):
        for label in ("Not Saved", "NOT SAVED", "Never Saved", "Error Saved", "Saved failed", "Saved"):
            with self.subTest(label=label):
                evidence = positive_evidence()
                evidence.update(after_ocr=words(label), after_ocr_observed=words(label))
                with self.assertRaisesRegex(suite.SuiteFailure, "Save-to-Saved evidence"):
                    suite.validate_positive(evidence, IDENTITY, SHA, 38)

    def test_uncertain_leading_negation_is_not_removed_from_success_evidence(self):
        evidence = positive_evidence()
        observed = words("Not Saved by")
        observed[0]["confidence"] = 5.0
        evidence.update(after_ocr=observed, after_ocr_observed=observed)
        with self.assertRaisesRegex(suite.SuiteFailure, "Save-to-Saved evidence"):
            suite.validate_positive(evidence, IDENTITY, SHA, 38)

    def test_initial_save_is_a_label_prefix(self):
        evidence = positive_evidence()
        evidence["initial_ocr"] = words("Not Save")
        with self.assertRaisesRegex(suite.SuiteFailure, "Save-to-Saved evidence"):
            suite.validate_positive(evidence, IDENTITY, SHA, 38)

    def test_loose_detector_location_and_size_are_not_a_valid_layout(self):
        for box in ([641, 25, 775, 59], [550, 70, 750, 130]):
            with self.subTest(box=box):
                evidence = positive_evidence()
                evidence.update(save_button_box=box,
                                save_button_layout=suite.gui.button_layout(box))
                with self.assertRaisesRegex(suite.SuiteFailure, "expected button layout"):
                    suite.validate_positive(evidence, IDENTITY, SHA, 38)

    def test_unchanged_success_words_do_not_hide_a_forged_layout(self):
        evidence = positive_evidence()
        evidence["save_button_box"] = [641, 25, 775, 59]
        with self.assertRaisesRegex(suite.SuiteFailure, "layout diagnosis"):
            suite.validate_positive(evidence, IDENTITY, SHA, 38)


class NegativeAdmissionTests(unittest.TestCase):
    def validate(self, evidence, scenario):
        suite.validate_negative(evidence, scenario, IDENTITY, SHA, 38)

    def test_all_precisely_observed_failures_are_accepted(self):
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
            uncertain = words(scenario["observed_label"])
            uncertain[0]["confidence"] = 19.9
            nonfinite = words(scenario["observed_label"])
            nonfinite[0]["confidence"] = float("nan")
            for observed in (None, [], words("error"), words("Unsaved"), uncertain, nonfinite):
                with self.subTest(scenario=scenario["name"], observed=observed):
                    evidence = negative_evidence(scenario)
                    evidence[scenario["observation"]] = observed
                    with self.assertRaises(suite.SuiteFailure):
                        self.validate(evidence, scenario)

    def test_completion_negatives_require_initial_save_and_real_pixel_change(self):
        for scenario in (case for case in suite.SCENARIOS if case["mouse"]):
            for change in ({"initial_ocr": []}, {"initial_ocr": words("Saving")},
                           {"changed_button_pixels": 0}, {"changed_button_pixels": 37}):
                with self.subTest(scenario=scenario["name"], change=change):
                    evidence = negative_evidence(scenario)
                    evidence.update(change)
                    with self.assertRaises(suite.SuiteFailure):
                        self.validate(evidence, scenario)

    def test_conflicting_success_observation_is_not_expected_failure(self):
        for scenario in suite.SCENARIOS:
            evidence = negative_evidence(scenario)
            evidence[scenario["observation"]] += words(scenario["forbidden_label"])
            with self.subTest(scenario=scenario["name"]), self.assertRaises(suite.SuiteFailure):
                self.validate(evidence, scenario)

    def test_layout_diagnosis_must_match_independently_recomputed_edges(self):
        for scenario in suite.SCENARIOS:
            original = negative_evidence(scenario)["save_button_layout"]
            diagnostics = (
                None,
                dict(original, matches=not original["matches"]),
                dict(original, tolerance_pixels=999),
                dict(original, edge_deltas=[0, 0, 0, 0]),
                dict(original, expected_box=[0, 0, 134, 34]),
                dict(original, observed_box=[641, 26, 775, 59]),
            )
            for report in diagnostics:
                with self.subTest(scenario=scenario["name"], report=report):
                    evidence = negative_evidence(scenario)
                    evidence["save_button_layout"] = report
                    with self.assertRaisesRegex(suite.SuiteFailure, "layout diagnosis"):
                        self.validate(evidence, scenario)

    def test_text_negatives_require_the_original_button_geometry(self):
        for scenario in suite.SCENARIOS[:2]:
            for box in ([641, 25, 775, 59], [550, 70, 750, 130]):
                with self.subTest(scenario=scenario["name"], box=box):
                    evidence = negative_evidence(scenario)
                    evidence.update(save_button_box=box,
                                    save_button_layout=suite.gui.button_layout(box))
                    with self.assertRaisesRegex(suite.SuiteFailure, "unrelated layout defect"):
                        self.validate(evidence, scenario)

    def test_layout_negative_rejects_normal_wrong_direction_and_changed_size(self):
        scenario = suite.SCENARIOS[2]
        for box in ([721, 25, 855, 59], [761, 25, 895, 59],
                    [641, 65, 775, 99], [681, 25, 815, 59],
                    [636, 25, 780, 59]):
            with self.subTest(box=box):
                evidence = negative_evidence(scenario)
                evidence.update(save_button_box=box,
                                save_button_layout=suite.gui.button_layout(box))
                with self.assertRaises(suite.SuiteFailure):
                    self.validate(evidence, scenario)

    def test_layout_negative_requires_successful_initial_save_ocr(self):
        scenario = suite.SCENARIOS[2]
        evidence = negative_evidence(scenario)
        evidence["initial_ocr"] = []
        with self.assertRaisesRegex(suite.SuiteFailure, "unchanged Save label"):
            self.validate(evidence, scenario)

    def test_nonvisual_exception_is_not_swallowed(self):
        args = SimpleNamespace(source_sha=SHA, timeout=12, minimum_changed=38)
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(suite.gui, "run", side_effect=OSError("missing dependency")):
                with self.assertRaisesRegex(OSError, "missing dependency"):
                    suite.execute_driver(args, Path("/unused/native-gui"), Path(directory))


class PlacementAdmissionTests(unittest.TestCase):
    def validate(self, evidence, scenario):
        if scenario is None:
            suite.validate_positive(evidence, IDENTITY, SHA, 38)
        else:
            suite.validate_negative(evidence, scenario, IDENTITY, SHA, 38)

    def all_cases(self):
        return [(positive_evidence(), None)] + [
            (negative_evidence(scenario), scenario) for scenario in suite.SCENARIOS]

    def test_every_case_requires_a_real_nonzero_placement_record(self):
        for original, scenario in self.all_cases():
            invalid = (
                {"window_rect": [0, 0, 928, 608]},
                {"window_rect": [127, 83, True, 608]},
                {"window_placement": None},
                {"window_placement": {}},
            )
            for update in invalid:
                with self.subTest(scenario=scenario, update=update):
                    evidence = deepcopy(original)
                    evidence.update(update)
                    with self.assertRaises(suite.SuiteFailure):
                        self.validate(evidence, scenario)
            for key, value in (
                    ("initial_rect", [0, 0, 927, 608]),
                    ("initial_rect", [0, 0]),
                    ("requested_origin", [0, 0]),
                    ("requested_origin", [127.0, 83]),
                    ("observed_rect", [128, 83, 928, 608]),
                    ("observed_rect", [127, 83, 928, 607]),
                    ("screen_size", [1000, 900]),
                    ("screen_size", [1280, 600]),
                    ("screen_size", [1280, True]),
                    ("screen_size", None)):
                with self.subTest(scenario=scenario, field=key, value=value):
                    evidence = deepcopy(original)
                    evidence["window_placement"][key] = value
                    with self.assertRaises(suite.SuiteFailure):
                        self.validate(evidence, scenario)

    def test_matching_forged_origin_in_all_rectangles_is_still_rejected(self):
        for original, scenario in self.all_cases():
            evidence = deepcopy(original)
            evidence["window_rect"] = [0, 0, 928, 608]
            evidence["window_placement"].update(
                requested_origin=[0, 0], observed_rect=[0, 0, 928, 608])
            with self.subTest(scenario=scenario), self.assertRaisesRegex(
                    suite.SuiteFailure, "nonzero screen origin"):
                self.validate(evidence, scenario)

    def test_clicking_cases_require_translated_and_observed_pointer_coordinates(self):
        for original, scenario in self.all_cases():
            if scenario is not None and not scenario["mouse"]:
                continue
            for key in ("hover_mouse", "save_click"):
                evidence = deepcopy(original)
                evidence.pop(key)
                with self.subTest(scenario=scenario, missing=key), self.assertRaisesRegex(
                        suite.SuiteFailure, key):
                    self.validate(evidence, scenario)
                for field, value in (
                        ("action", "left_click" if key == "hover_mouse" else "move"),
                        ("window_rect", [0, 0, 928, 608]),
                        ("client", [787, 42]),
                        ("client", [788.0, 42]),
                        ("screen", [788, 42]),
                        ("screen", [916, 125]),
                        ("observed_screen", [915, 126]),
                        ("observed_client", [788, 43]),
                        ("observed_client", None)):
                    with self.subTest(scenario=scenario, action=key, field=field, value=value):
                        evidence = deepcopy(original)
                        evidence[key][field] = value
                        with self.assertRaisesRegex(suite.SuiteFailure, key):
                            self.validate(evidence, scenario)

    def test_preclick_negatives_must_not_record_mouse_actions(self):
        for scenario in (item for item in suite.SCENARIOS if not item["mouse"]):
            for key in ("hover_mouse", "save_click"):
                for value in (None, mouse_evidence("left_click", [721, 25, 855, 59])):
                    evidence = negative_evidence(scenario)
                    evidence[key] = value
                    with self.subTest(scenario=scenario, key=key, value=value), \
                         self.assertRaisesRegex(suite.SuiteFailure, "pre-click negative"):
                        self.validate(evidence, scenario)


@unittest.skipIf(Image is None, "Pillow is required for retained-root pixel regressions")
class DesktopScreenshotTests(unittest.TestCase):
    """Synthetic images exercise rejection only; they do not qualify the GUI."""

    def write_frames(self, directory, clicked):
        for index, name in enumerate(("before", "hover", "after") if clicked else ("before",)):
            client = Image.new("RGB", (928, 608), (15 + index, 23, 42))
            draw = ImageDraw.Draw(client)
            draw.rectangle((10, 12, 300, 30), fill=(250, 100, 40))
            draw.rectangle((721, 25, 854, 58), fill=(37, 99 + index, 235))
            desktop = Image.new("RGB", tuple(SCREEN_SIZE), (192, 0, 192))
            desktop.paste(client, (127, 83))
            client.save(directory / f"{name}.png")
            desktop.save(directory / f"desktop_{name}.png")

    def test_each_case_verifies_its_actual_capture_phases(self):
        cases = [(positive_evidence(), True)] + [
            (negative_evidence(case), case["mouse"]) for case in suite.SCENARIOS]
        for evidence, clicked in cases:
            with self.subTest(clicked=clicked, result=evidence["result"]), \
                 tempfile.TemporaryDirectory() as temporary:
                directory = Path(temporary)
                self.write_frames(directory, clicked)
                records = suite.verify_desktop_screenshots(directory, evidence, clicked)
                self.assertEqual([record["frame"] for record in records],
                                 ["before", "hover", "after"] if clicked else ["before"])
                self.assertTrue(all(record["crop_rect"] == [127, 83, 1055, 691] and
                                    record["pixels_match"] is True for record in records))

    def test_missing_client_or_root_capture_is_not_reconstructed(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.write_frames(directory, True)
            for name in ("before", "hover", "after"):
                for prefix in ("", "desktop_"):
                    path = directory / f"{prefix}{name}.png"
                    original = path.read_bytes()
                    path.unlink()
                    with self.subTest(file=path.name), self.assertRaisesRegex(
                            suite.SuiteFailure, "missing retained"):
                        suite.verify_desktop_screenshots(directory, positive_evidence(), True)
                    path.write_bytes(original)

    def test_one_changed_pixel_in_each_root_crop_fails(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.write_frames(directory, True)
            for name in ("before", "hover", "after"):
                path = directory / f"desktop_{name}.png"
                original = path.read_bytes()
                with Image.open(path) as image:
                    changed = image.copy()
                changed.putpixel((137, 93), (0, 255, 0))
                changed.save(path)
                with self.subTest(frame=name), self.assertRaisesRegex(
                        suite.SuiteFailure, "pixels do not match"):
                    suite.verify_desktop_screenshots(directory, positive_evidence(), True)
                path.write_bytes(original)

    def test_stale_or_origin_zero_crops_cannot_claim_nonzero_placement(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.write_frames(directory, True)
            (directory / "after.png").write_bytes((directory / "hover.png").read_bytes())
            with self.assertRaisesRegex(suite.SuiteFailure, "pixels do not match"):
                suite.verify_desktop_screenshots(directory, positive_evidence(), True)
            self.write_frames(directory, True)
            with Image.open(directory / "before.png") as client:
                desktop = Image.new("RGB", tuple(SCREEN_SIZE), (192, 0, 192))
                desktop.paste(client, (0, 0))
                desktop.save(directory / "desktop_before.png")
            with self.assertRaisesRegex(suite.SuiteFailure, "pixels do not match"):
                suite.verify_desktop_screenshots(directory, positive_evidence(), True)

    def test_wrong_size_or_converted_pixel_format_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            for name, size, mode in (
                    ("before.png", (927, 608), "RGB"),
                    ("desktop_before.png", (1279, 900), "RGB"),
                    ("desktop_before.png", tuple(SCREEN_SIZE), "RGBA")):
                self.write_frames(directory, False)
                Image.new(mode, size).save(directory / name)
                with self.subTest(file=name, size=size, mode=mode), \
                     self.assertRaises(suite.SuiteFailure):
                    suite.verify_desktop_screenshots(directory, positive_evidence(), False)


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
        (assets / "editor.css").write_text(
            '#title { width: 680px; }\n#save { color: white; }\n', encoding="utf-8")
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

    @unittest.skipIf(Image is None, "Pillow is required for retained-root pixel regressions")
    def test_success_metadata_without_desktop_captures_cannot_pass_the_suite(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            exe = self.create_files(root / "runtime")
            identity = suite.runtime_identity(exe)
            evidence = positive_evidence()
            evidence["binary_sha256"] = identity["binary"]
            out = root / "evidence"
            args = SimpleNamespace(exe=str(exe), source_sha=SHA, out=str(out),
                                   timeout=12, minimum_changed=38)
            with patch.object(suite, "execute_driver", return_value=(evidence, False)), \
                 self.assertRaisesRegex(suite.SuiteFailure, "missing retained"):
                suite.run_suite(args)
            summary = json.loads((out / "suite.json").read_text(encoding="utf-8"))
            self.assertEqual(summary["result"], "FAIL")
            self.assertNotIn("positive_result", summary)
            self.assertEqual(summary["positive_window_placement"], evidence["window_placement"])
            self.assertEqual(summary["positive_save_click"], evidence["save_click"])
            self.assertTrue(summary["original_runtime_unchanged"])


if __name__ == "__main__":
    unittest.main(verbosity=1)

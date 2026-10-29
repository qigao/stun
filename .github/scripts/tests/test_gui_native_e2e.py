#!/usr/bin/env python3
"""GUI driver regressions; neither synthetic pixels nor X11 fixtures qualify Stun.

Set STUN_GUI_X11_TESTS=1 under Xvfb to require the real Xlib/capture/input tests.
"""
import json
import os
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import gui_native_e2e as gui


class PixelAccessor:
    def __init__(self, pixels, default):
        self.pixels, self.default = pixels, default

    def __getitem__(self, xy):
        return self.pixels.get(xy, self.default)


class Frame:
    def __init__(self, width=928, height=608, pixels=None,
                 default=(15, 23, 42)):
        self.width, self.height = width, height
        self.size = (width, height)
        self.pixels = pixels or {}
        self.default = default

    def load(self):
        return PixelAccessor(self.pixels, self.default)


def button_frame(x=713, y=25, width=136, height=36):
    pixels = {(i, j): (37, 99, 235)
              for i in range(x, x + width) for j in range(y, y + height)}
    return Frame(pixels=pixels)


class GuiPixelTests(unittest.TestCase):
    def test_actual_css_blue_is_detected(self):
        self.assertEqual(gui.detect_button(button_frame()),
                         (713, 25, 849, 61))

    def test_missing_button_fails(self):
        with self.assertRaises(gui.VisualFailure):
            gui.detect_button(Frame())

    def test_misplaced_button_is_not_accepted(self):
        with self.assertRaises(gui.VisualFailure):
            gui.detect_button(button_frame(x=20, y=25))

    def test_tiny_blue_noise_is_not_a_button(self):
        with self.assertRaises(gui.VisualFailure):
            gui.detect_button(button_frame(width=8, height=8))

    def test_wrong_window_dimensions_are_rejected(self):
        with self.assertRaises(gui.VisualFailure):
            gui.detect_button(Frame(width=320, height=180))

    def test_hover_only_does_not_pass_click(self):
        a, b = button_frame(), button_frame()
        self.assertEqual(gui.count_changed(a, b, (713, 25, 849, 61)), 0)

    def test_real_ink_mutation_changes_button_pixels(self):
        a, b = button_frame(), button_frame()
        for x in range(756, 806):
            for y in range(36, 50):
                b.pixels[x, y] = (255, 255, 255)
        self.assertEqual(
            gui.count_changed(a, b, (713, 25, 849, 61)), 700)

    def test_subthreshold_noise_is_ignored(self):
        a, b = button_frame(), button_frame()
        b.pixels[760, 40] = (40, 103, 238)
        self.assertEqual(
            gui.count_changed(a, b, (713, 25, 849, 61)), 0)

    def test_invalid_pixel_roi_is_rejected(self):
        with self.assertRaises(gui.VisualFailure):
            gui.count_changed(button_frame(), button_frame(),
                              (700, 10, 1200, 20))

    def test_changed_frame_size_is_rejected(self):
        with self.assertRaises(gui.VisualFailure):
            gui.count_changed(button_frame(),
                              Frame(width=927, height=608),
                              (700, 20, 820, 50))

    def test_renderer_is_real_x11_only(self):
        import inspect
        body = inspect.getsource(gui.run)
        self.assertIn('display.Display()', body)
        self.assertIn('subprocess.Popen(', body)
        self.assertIn('capture(disp, target)', body)
        self.assertIn('mouse(disp, target, *center, click=True)', body)
        self.assertNotIn('"--smoke"', body)

    def test_binary_and_pixel_sources_are_separate(self):
        self.assertTrue(gui.is_button_blue((37, 99, 235)))
        self.assertTrue(gui.is_button_blue((59, 130, 246)))
        self.assertFalse(gui.is_button_blue((15, 23, 42)))
        self.assertFalse(gui.is_button_blue((255, 255, 255)))


class GuiOcrTests(unittest.TestCase):
    @staticmethod
    def result(text):
        header = "level\tpage_num\tblock_num\tpar_num\tline_num\tword_num\tleft\ttop\twidth\theight\tconf\ttext\n"
        rows = [f"5\t1\t1\t1\t1\t{i}\t10\t10\t50\t20\t95\t{word}\n"
                for i, word in enumerate(text.split(), 1)]
        return SimpleNamespace(returncode=0, stdout=header + "".join(rows), stderr="")

    def test_completed_save_matches_visible_word(self):
        with patch.object(gui.subprocess, "run", return_value=self.result(
                "Saved by native document service")):
            self.assertEqual(gui.local_ocr("unused.png", "Saved")[0]["text"], "Saved")

    def test_pending_error_and_unsaved_are_not_completion(self):
        for text in ("Saving...", "Save failed", "Unsaved", "Save", "S a v e d"):
            with self.subTest(text=text), patch.object(
                    gui.subprocess, "run", return_value=self.result(text)):
                with self.assertRaises(gui.OcrMismatch):
                    gui.local_ocr("unused.png", "Saved")

    def test_preflight_failure_preserves_evidence(self):
        with tempfile.TemporaryDirectory() as directory, patch.dict(os.environ, {}, clear=True):
            args = SimpleNamespace(exe="/missing/native-gui", source_sha="a" * 40,
                                   out=directory, timeout=1, minimum_changed=38)
            with self.assertRaises(gui.VisualFailure):
                gui.run(args)
            evidence = json.loads((Path(directory) / "evidence.json").read_text())
            self.assertEqual(evidence["result"], "FAIL")
            self.assertFalse(evidence["real_mouse_event"])
            self.assertIn("error", evidence)


@unittest.skipUnless(os.environ.get("STUN_GUI_X11_TESTS") == "1",
                     "native X11 driver checks require STUN_GUI_X11_TESTS=1 and Xvfb")
class NativeX11DriverTests(unittest.TestCase):
    def setUp(self):
        from Xlib import X, display
        self.disp = display.Display()
        self.addCleanup(self.disp.close)
        screen = self.disp.screen()
        # A parent at a nonzero origin also catches accidentally using the
        # child geometry's parent-relative x/y as screen coordinates.
        parent = screen.root.create_window(
            47, 29, 1100, 800, 0, screen.root_depth,
            X.InputOutput, X.CopyFromParent, background_pixel=0xFF00FF)
        self.addCleanup(parent.destroy)
        self.window = parent.create_window(
            80, 54, 928, 608, 0, screen.root_depth,
            X.InputOutput, X.CopyFromParent, background_pixel=0x2563EB,
            event_mask=X.ButtonPressMask | X.ButtonReleaseMask)
        self.window.set_wm_name("Stun GUI driver protocol test")
        self.window.map()
        parent.map()
        self.disp.sync()

    def test_native_client_origin_and_screen_capture(self):
        self.assertEqual(gui.window_rect(self.disp, self.window), (127, 83, 928, 608))
        frame = gui.capture(self.disp, self.window)
        self.assertEqual(frame.size, (928, 608))
        self.assertEqual(frame.getpixel((10, 10)), (37, 99, 235))

    def test_real_mouse_click_reaches_client_coordinates(self):
        from Xlib import X
        gui.mouse(self.disp, self.window, 37, 41, click=True)
        pointer = self.disp.screen().root.query_pointer()
        self.assertEqual((pointer.root_x, pointer.root_y), (164, 124))
        buttons = []
        while self.disp.pending_events():
            event = self.disp.next_event()
            if event.type in (X.ButtonPress, X.ButtonRelease):
                buttons.append((event.type, event.event_x, event.event_y))
        self.assertEqual(buttons, [(X.ButtonPress, 37, 41), (X.ButtonRelease, 37, 41)])


if __name__ == "__main__":
    unittest.main(verbosity=1)

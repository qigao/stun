#!/usr/bin/env python3
"""Deterministic screenshot-oracle tests: NOT a real Stun GUI qualification."""
from pathlib import Path
import sys
import unittest

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

    def test_x11_client_origin_translates_window_to_root(self):
        # XTranslateCoordinates(source=window, destination=root, 0, 0).
        # Reversing the two produces negative screen coordinates.
        class Geometry:
            width = 928
            height = 608
        class Root:
            pass
        root = Root()
        class Window:
            def get_geometry(self):
                return Geometry()
            def translate_coords(self, destination, x, y):
                self_was_source = destination is root and (x, y) == (0, 0)
                if not self_was_source:
                    raise AssertionError("X11 translation direction inverted")
                return type("Position", (), {"x": 127, "y": 83})()
        disp = type("Display", (), {
            "screen": lambda _self: type("Screen", (), {"root": root})()
        })()
        self.assertEqual(gui.window_rect(disp, Window()), (127, 83, 928, 608))

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


if __name__ == "__main__":
    unittest.main(verbosity=1)

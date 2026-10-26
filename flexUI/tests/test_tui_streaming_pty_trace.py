#!/usr/bin/env python3
"""Regression for #270's bounded, latest-event POSIX PTY trace reader.

The real PTY acceptance still independently requires visible terminal cells.
This unit gate protects its out-of-band diagnostic provenance when many wheel
and Owner-frame records push the newest evidence beyond the first 16 KiB.
"""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_tui_streaming_pty import (PTY_TRACE_LIMIT,
                                    read_owner_trace_lines,
                                    require_navigation_provenance)


class PtyTraceReaderTest(unittest.TestCase):
    def test_latest_owner_frame_after_first_16_kib(self):
        older = (b"command=2 following=0 retained=12 anchor=10 "
                 b"projected=10 epoch=7\n")
        newest = (b"owner_view_epoch=57 command=5 following=0 "
                  b"retained=12 anchor=37 projected=37 "
                  b"cell_mode=1 cell_row=22\n")
        payload = older * 300 + newest
        self.assertGreater(len(payload), 16 * 1024)
        self.assertLess(len(payload), PTY_TRACE_LIMIT)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "pty-trace"
            path.write_bytes(payload)
            records = read_owner_trace_lines(path)
            self.assertEqual(records[-1], newest.rstrip(b"\n"))
            self.assertEqual(len(records), 301)

    def test_trace_overflow_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "pty-trace"
            path.write_bytes(b"x" * (PTY_TRACE_LIMIT + 1))
            with self.assertRaisesRegex(AssertionError, "evidence incomplete"):
                read_owner_trace_lines(path)

    def test_trace_at_bound_is_accepted(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "pty-trace"
            path.write_bytes(b"x" * (PTY_TRACE_LIMIT - 1) + b"\n")
            records = read_owner_trace_lines(path)
            self.assertEqual(len(records), 1)
            self.assertEqual(len(records[0]), PTY_TRACE_LIMIT - 1)


class PtyNavigationEpochTest(unittest.TestCase):
    @staticmethod
    def records(epoch=9, frame_epoch=None, command=5, follow=0,
                cell_mode=1, geometry_epoch=None):
        if frame_epoch is None:
            frame_epoch = epoch
        if geometry_epoch is None:
            geometry_epoch = epoch
        return [
            ("command=%d following=1 retained=6 anchor=0 "
             "projected=0 epoch=%d" % (command, epoch)).encode("ascii"),
            ("owner_view_epoch=%d command=%d following=%d "
             "retained=6 anchor=1 projected=1 scroll_y=0.0 max_y=100.0 "
             "frame=80x24 valid=1 cell_mode=%d cell_row=22" %
             (frame_epoch, command, follow, cell_mode)).encode("ascii"),
            ("virtual_geometry epoch=%d enabled=1 visible=5 children=7 "
             "leading=100.0 trailing=0.0 first=1" %
             geometry_epoch).encode("ascii"),
        ]

    def test_new_command_owner_cells_and_geometry_same_epoch(self):
        self.assertEqual(
            require_navigation_provenance(self.records(), 8, 5, False, True),
            9)

    def test_repeated_home_cannot_use_old_command(self):
        with self.assertRaisesRegex(AssertionError, "no new on_key admission"):
            require_navigation_provenance(
                self.records(epoch=8), 8, 5, False, True)

    def test_fresh_command_cannot_use_old_owner_frame(self):
        with self.assertRaisesRegex(
                AssertionError, "belongs to another input epoch"):
            require_navigation_provenance(
                self.records(frame_epoch=8), 8, 5, False, True)

    def test_fresh_command_rejects_wrong_follow_state(self):
        with self.assertRaisesRegex(AssertionError, "owner state mismatch"):
            require_navigation_provenance(
                self.records(follow=1), 8, 5, False, True)

    def test_correct_owner_with_stale_box_geometry_is_inconclusive(self):
        with self.assertRaisesRegex(AssertionError, "geometry is stale"):
            require_navigation_provenance(
                self.records(geometry_epoch=8), 8, 5, False, True)

    def test_wrong_owned_cell_mode_does_not_pass(self):
        with self.assertRaisesRegex(AssertionError, "Cell Buffer disagrees"):
            require_navigation_provenance(
                self.records(cell_mode=0), 8, 5, False, True)

    def test_last_command_not_expected_is_rejected(self):
        with self.assertRaisesRegex(AssertionError, "wrong latest command"):
            require_navigation_provenance(
                self.records(command=4), 8, 5, False, True)

    def test_saturated_out_of_band_trace_is_not_valid_proof(self):
        records = self.records()[:1] * 64 + self.records()[1:]
        with self.assertRaisesRegex(AssertionError, "trace saturated"):
            require_navigation_provenance(records, 8, 5, False, True)



if __name__ == "__main__":
    unittest.main()

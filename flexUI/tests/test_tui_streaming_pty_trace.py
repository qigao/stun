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
from test_tui_streaming_pty import PTY_TRACE_LIMIT, read_owner_trace_lines


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


if __name__ == "__main__":
    unittest.main()

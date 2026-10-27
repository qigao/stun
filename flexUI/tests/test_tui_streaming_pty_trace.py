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
from test_tui_streaming_pty import (PTY_TRACE_LIMIT, fnv1a64,
                                    is_pending_native_receipt_error,
                                    read_owner_trace_lines,
                                    require_navigation_provenance,
                                    require_render_receipt)


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



class PtyNativeWriteReceiptTest(unittest.TestCase):
    @staticmethod
    def diff():
        return b"\x1b[0m\x1b[23;1HRetained 7/12 | scrollback | 80x24 | "

    @classmethod
    def records(cls, epoch=9, front_mode=1, kind="render_commit",
                payload=None):
        data = cls.diff() if payload is None else payload
        prefix = data[:16].hex()
        before = (b"owner_view_epoch=9 command=5 following=0 "
                  b"cell_mode=1 cell_row=22")
        receipt = (
            "%s epoch=%d bytes=%d fnv64=%016x prefix=%s "
            "cell_mode=%d cell_row=22 error=0" %
            (kind, epoch, len(data), fnv1a64(data), prefix, front_mode)
        ).encode("ascii")
        return [before, receipt]

    def test_matches_actual_captured_serialized_diff(self):
        data = self.diff()
        raw = bytearray(b"previous frames") + data + b"\x1b[0m"
        self.assertEqual(
            require_render_receipt(
                self.records(), 9, False, raw, len(b"previous frames")),
            len(b"previous frames"))

    def test_stale_commit_cannot_qualify_new_home(self):
        with self.assertRaisesRegex(AssertionError, "stale epoch"):
            require_render_receipt(
                self.records(epoch=8), 9, False, self.diff(), 0)

    def test_native_write_failure_cannot_qualify(self):
        with self.assertRaisesRegex(AssertionError, "did not commit"):
            require_render_receipt(
                self.records(kind="render_failed"), 9, False, self.diff(), 0)

    def test_correct_model_with_wrong_committed_cells_fails(self):
        with self.assertRaisesRegex(AssertionError, "committed front cells"):
            require_render_receipt(
                self.records(front_mode=0), 9, False, self.diff(), 0)

    def test_matching_prefix_but_missing_full_bytes_fails(self):
        changed = bytearray(self.diff())
        changed[-1] ^= 1
        with self.assertRaisesRegex(AssertionError, "absent from physical PTY"):
            require_render_receipt(
                self.records(), 9, False, changed, 0)

    def test_old_bytes_before_navigation_do_not_qualify(self):
        raw = bytearray(self.diff() + b"unrelated")
        with self.assertRaisesRegex(AssertionError, "absent from physical PTY"):
            require_render_receipt(
                self.records(), 9, False, raw, len(self.diff()))

    def test_receipt_preceding_owner_frame_is_invalid(self):
        with self.assertRaisesRegex(AssertionError, "before the requested"):
            require_render_receipt(
                self.records()[::-1], 9, False, self.diff(), 0)

    def test_empty_native_diff_is_not_evidence(self):
        with self.assertRaisesRegex(AssertionError, "no bounded VT diff"):
            require_render_receipt(
                self.records(payload=b""), 9, False, b"", 0)

    def test_integrated_command_owner_write_and_output(self):
        raw = bytearray(b"already drained") + self.diff()
        records = PtyNavigationEpochTest.records() + self.records()[1:]
        self.assertEqual(
            require_navigation_provenance(
                records, 8, 5, False, True, raw, len(b"already drained")), 9)




class PtyNativeReceiptSynchronizationPolicyTest(unittest.TestCase):
    def test_only_pending_post_write_evidence_may_be_awaited(self):
        # Genuine Home/End is already required to be physically visible.
        for message in (
            "no native terminal diff-write receipt",
            "native diff-write receipt belongs to stale epoch: b'epoch=8'",
            "committed Tango VT diff bytes absent from physical PTY output: epoch=9",
        ):
            self.assertTrue(is_pending_native_receipt_error(
                AssertionError(message)), message)

    def test_native_failure_and_wrong_owner_are_never_retryable(self):
        # A bounded evidence-propagation window must not disguise an
        # incorrect command, mismatched front cells, or rejected native I/O.
        for message in (
            "native Tango render did not commit",
            "native committed front cells differ from Owner or write failed",
            "post-Box owner state mismatch",
            "no new on_key admission since command epoch 8",
            "malformed native diff-write receipt",
            "PTY command trace saturated; latest input provenance unknown",
        ):
            self.assertFalse(is_pending_native_receipt_error(
                AssertionError(message)), message)


if __name__ == "__main__":
    unittest.main()

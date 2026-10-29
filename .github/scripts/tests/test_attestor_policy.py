#!/usr/bin/env python3
"""Deterministic, no-network attestor policy-bundle pinning tests."""
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / ".github/scripts"))
from attestor_policy import (  # noqa: E402
    MAX_POLICY_FILE_BYTES, POLICY_FILES, PolicyIntegrityError,
    policy_bundle_sha256, verify_policy_bundle,
)
from ci_required_matrix import PUBLIC, RUNTIME, WORKFLOWS  # noqa: E402


class PinnedPolicyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="stun-policy-release-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "approved-policy"
        for relative in POLICY_FILES:
            target = self.root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, target)
        self.approved = policy_bundle_sha256(self.root)

    def test_approved_digest_is_reproducible(self):
        self.assertEqual(policy_bundle_sha256(self.root), self.approved)
        self.assertEqual(verify_policy_bundle(self.root, self.approved),
                         self.approved)
        self.assertEqual(len(self.approved), 64)

    def test_operator_must_supply_valid_digest_independently(self):
        for value in (None, "", "0", "a" * 63, "x" * 64, True, 123):
            with self.subTest(value=value), self.assertRaises(
                PolicyIntegrityError
            ):
                verify_policy_bundle(self.root, value)

    def test_changed_policy_file_invalidates_approved_digest(self):
        target = self.root / ".github/scripts/ci_required_matrix.py"
        target.write_bytes(target.read_bytes() + b"\n# changed after review\n")
        with self.assertRaises(PolicyIntegrityError):
            verify_policy_bundle(self.root, self.approved)

    def test_modified_workflow_filter_invalidates_digest(self):
        target = self.root / ".github/workflows/runtime-conformance.yml"
        target.write_bytes(target.read_bytes() + b"\n# changed workflow\n")
        with self.assertRaises(PolicyIntegrityError):
            verify_policy_bundle(self.root, self.approved)

    def test_missing_critical_module_fails_closed(self):
        (self.root / ".github/scripts/verify_exact_head_ci.py").unlink()
        with self.assertRaises(PolicyIntegrityError):
            verify_policy_bundle(self.root, self.approved)

    def test_linked_critical_file_fails_closed(self):
        linked = self.root / ".github/scripts/ci_required_matrix.py"
        linked.unlink()
        linked.symlink_to(ROOT / ".github/scripts/ci_required_matrix.py")
        with self.assertRaises(PolicyIntegrityError):
            verify_policy_bundle(self.root, self.approved)

    def test_symlinked_parent_directory_fails_closed(self):
        folder = self.root / ".github/scripts"
        original = self.root / ".github/scripts-real"
        folder.rename(original)
        folder.symlink_to(original, target_is_directory=True)
        with self.assertRaises(PolicyIntegrityError):
            verify_policy_bundle(self.root, self.approved)

    def test_symlinked_policy_root_fails_closed(self):
        linked = Path(self.temp.name) / "alias"
        linked.symlink_to(self.root, target_is_directory=True)
        with self.assertRaises(PolicyIntegrityError):
            verify_policy_bundle(linked, self.approved)

    def test_relative_policy_root_fails_closed(self):
        with self.assertRaises(PolicyIntegrityError):
            verify_policy_bundle(Path("untrusted/relative"), self.approved)

    def test_file_empty_or_oversized_fails_closed(self):
        name = self.root / ".github/scripts/attestor_policy.py"
        name.write_bytes(b"")
        with self.assertRaises(PolicyIntegrityError):
            policy_bundle_sha256(self.root)
        name.write_bytes(b"x" * (MAX_POLICY_FILE_BYTES + 1))
        with self.assertRaises(PolicyIntegrityError):
            policy_bundle_sha256(self.root)

    def test_unreviewed_imported_module_cannot_match_bundle(self):
        loaded = {
            ".github/scripts/attestor_worker.py":
                str(ROOT / ".github/scripts/attestor_worker.py")
        }
        with self.assertRaises(PolicyIntegrityError):
            verify_policy_bundle(self.root, self.approved,
                                 loaded_module_paths=loaded)

    def test_runtime_modules_must_be_known_reviewed_files(self):
        with self.assertRaises(PolicyIntegrityError):
            verify_policy_bundle(self.root, self.approved,
                                 loaded_module_paths={
                                    ".github/scripts/unreviewed.py":
                                    str(self.root / "unreviewed.py")
                                 })

    def test_all_existing_native_workflows_are_pinned(self):
        relevant = {PUBLIC, RUNTIME}
        relevant.update(".github/workflows/" + x for x in WORKFLOWS)
        self.assertTrue(relevant.issubset(set(POLICY_FILES)))
        self.assertIn(".github/scripts/attestor_worker.py", POLICY_FILES)
        self.assertIn(".github/scripts/attestor_policy.py", POLICY_FILES)
        self.assertEqual(POLICY_FILES, tuple(sorted(set(POLICY_FILES))))

    def test_an_unrelated_file_does_not_change_digest(self):
        (self.root / ".github/scripts/unknown_notes.txt").write_text(
            "not loaded by trusted attestor", encoding="utf-8")
        self.assertEqual(policy_bundle_sha256(self.root), self.approved)


if __name__ == "__main__":
    unittest.main(verbosity=1)

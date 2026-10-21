#!/usr/bin/env python3
"""Offline fail-closed tests for isolated published SDK restore and Core ownership."""
import hashlib
import importlib.util
from pathlib import Path
import tempfile
import stat
import unittest
from unittest.mock import patch
import zipfile

SCRIPT = Path(__file__).resolve().parents[1] / "restore-flowchart-sdk.py"
spec = importlib.util.spec_from_file_location("flowchart_sdk_restore", SCRIPT)
restore = importlib.util.module_from_spec(spec)
spec.loader.exec_module(restore)


class SdkRestoreTests(unittest.TestCase):
    def test_official_checksum(self):
        with tempfile.TemporaryDirectory() as root:
            d = Path(root)
            archive = d / "salts.native.2.3.0-rc.17.nupkg"
            archive.write_bytes(b"real-byte-input")
            sha = hashlib.sha256(archive.read_bytes()).hexdigest()
            sums = d / "SHA256SUMS"
            sums.write_text(f"{sha} *{archive.name}\n", encoding="utf-8")
            self.assertEqual(restore.verify_official_digest(archive, sums), sha)
            sums.write_text(f"{'0' * 64} *{archive.name}\n", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "mismatch"):
                restore.verify_official_digest(archive, sums)
            sums.write_text(f"{sha} *{archive.name}\n{sha} *{archive.name}\n", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "ambiguous"):
                restore.verify_official_digest(archive, sums)

    def test_exact_installed_sdk_payload(self):
        with tempfile.TemporaryDirectory() as root:
            d = Path(root)
            archive = d / "sdk.nupkg"
            sdk = d / "installed"
            with zipfile.ZipFile(archive, "w") as z:
                z.writestr("README.md", "not an SDK path")
                z.writestr("sdk/linux-x64/lib/libtinytest.a", "official-content")
                z.writestr("sdk/win-x64/lib/libtinytest.a", "other platform")
            self.assertEqual(restore.extract_sdk_payload(archive, sdk), 1)
            self.assertEqual((sdk / "lib/libtinytest.a").read_bytes(), b"official-content")
            self.assertFalse((sdk / "README.md").exists())
            # A second package without any new installed payload must fail
            # closed rather than silently masquerade as another SDK component.
            with self.assertRaisesRegex(RuntimeError, "No installed linux-x64 SDK payload"):
                restore.extract_sdk_payload(archive, sdk)

            (sdk / "lib/libtinytest.a").write_bytes(b"tampered")
            with self.assertRaisesRegex(RuntimeError, "Conflicting"):
                restore.extract_sdk_payload(archive, sdk)

    def test_zip_path_traversal_is_rejected(self):
        with tempfile.TemporaryDirectory() as root:
            d = Path(root)
            archive = d / "attack.nupkg"
            with zipfile.ZipFile(archive, "w") as z:
                z.writestr("sdk/linux-x64/../../outside", "not allowed")
            with self.assertRaisesRegex(RuntimeError, "Unsafe SDK"):
                restore.extract_sdk_payload(archive, d / "installed")
            self.assertFalse((d / "outside").exists())

    def test_sdk_symlink_entry_is_rejected(self):
        with tempfile.TemporaryDirectory() as root:
            d = Path(root)
            archive = d / "symlink.nupkg"
            entry = zipfile.ZipInfo("sdk/linux-x64/lib/libevil.so")
            entry.create_system = 3
            entry.external_attr = (stat.S_IFLNK | 0o777) << 16
            with zipfile.ZipFile(archive, "w") as z:
                z.writestr(entry, "../../another-file")
            with self.assertRaisesRegex(RuntimeError, "Unsupported symlink"):
                restore.extract_sdk_payload(archive, d / "installed")
            self.assertFalse((d / "installed/lib/libevil.so").exists())

    def test_verified_core_aliases_become_canonical_symlinks(self):
        with tempfile.TemporaryDirectory() as root:
            d = Path(root)
            lib = d / "lib"
            lib.mkdir()
            payload = b"published ELF payload"
            (lib / "libsalts.so.2.3.0").write_bytes(payload)
            (lib / "libsalts.so.2.3").write_bytes(payload)
            (lib / "libsalts.so").write_bytes(payload)
            soname = "0x000000000000000e (SONAME) Library soname: [libsalts.so.2.3]"
            with patch.object(restore.subprocess, "check_output", return_value=soname):
                manifest = restore.canonicalize_core(d, "2.3.0-rc.17")
            self.assertEqual(manifest["soname"], "libsalts.so.2.3")
            self.assertTrue((lib / "libsalts.so").is_symlink())
            self.assertTrue((lib / "libsalts.so.2.3").is_symlink())
            self.assertEqual((lib / "libsalts.so").resolve(), (lib / "libsalts.so.2.3.0"))

    def test_conflicting_provider_rejected(self):
        with tempfile.TemporaryDirectory() as root:
            d = Path(root)
            lib = d / "lib"
            lib.mkdir()
            (lib / "libsalts.so.2.3.0").write_bytes(b"official")
            (lib / "libsalts.so.2.3").write_bytes(b"other")
            (lib / "libsalts.so").write_bytes(b"official")
            with patch.object(restore.subprocess, "check_output",
                              return_value="0x000000000000000e (SONAME) Library soname: [libsalts.so.2.3]"):
                with self.assertRaisesRegex(RuntimeError, "conflicting"):
                    restore.canonicalize_core(d, "2.3.0")


if __name__ == "__main__":
    unittest.main()

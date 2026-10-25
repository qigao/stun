#!/usr/bin/env python3
"""Small adversarial tests for the official cross-platform SDK admission gate."""
import hashlib
import importlib.util
import pathlib
import stat
import tempfile
import unittest
import warnings
import zipfile
from types import SimpleNamespace


SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "restore-flexui-tango-sdk.py"
spec = importlib.util.spec_from_file_location("restore_flexui_tango_sdk", SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class RestoreSDKTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        self.packages = self.root / "packages"
        self.salts = self.root / "salts.sums"
        self.utils = self.root / "utils.sums"
        self.sdk = self.root / "sdk"
        self.rid = "macos-arm64"

    def make_archive(self, name, entries, checksums):
        release = "v1.2.3"
        archive = self.packages / name / "1.2.3" / f"{name}.1.2.3.nupkg"
        archive.parent.mkdir(parents=True, exist_ok=True)
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", UserWarning)
            with zipfile.ZipFile(archive, "w") as z:
                for item in entries:
                    if len(item) == 2:
                        path, data = item
                        z.writestr(f"sdk/{self.rid}/{path}", data)
                    else:
                        path, data, mode = item
                        info = zipfile.ZipInfo(f"sdk/{self.rid}/{path}")
                        info.create_system = 3
                        info.external_attr = mode << 16
                        z.writestr(info, data)
        checksums.write_text(hashlib.sha256(archive.read_bytes()).hexdigest()
                             + " *" + archive.name + "\n", encoding="utf-8")
        return release, archive

    def args(self):
        return SimpleNamespace(packages=self.packages, sdk=self.sdk,
            rid=self.rid, salts_release="v1.2.3",
            salts_utils_release="v1.2.3",
            salts_checksums=self.salts, salts_utils_checksums=self.utils)

    def valid(self):
        self.make_archive("salts.native", [
            ("include/vstr.h", b"salts"),
            ("include/unicode/salts_unicode.h", b"unicode"),
            ("lib/cmake/Salts/SaltsConfig.cmake", b"salts-cmake")
        ], self.salts)
        self.make_archive("saltsutils.native", [
            ("lib/cmake/SaltsUtils/SaltsUtilsConfig.cmake", b"utils-cmake"),
            ("include/utils.h", b"utils")
        ], self.utils)

    def test_accepted_and_transactional(self):
        self.valid()
        module.restore(self.args())
        self.assertEqual((self.sdk / "include/vstr.h").read_bytes(), b"salts")
        self.assertTrue((self.sdk / "flexui-tango-sdk-provenance.json").is_file())

    def test_tampered_checksum_fails_without_install(self):
        self.valid()
        self.utils.write_text("0" * 64 + " *saltsutils.native.1.2.3.nupkg\n")
        with self.assertRaisesRegex(RuntimeError, "digest mismatch"):
            module.restore(self.args())
        self.assertFalse(self.sdk.exists())

    def test_invalid_rid_fails_before_extract(self):
        self.valid()
        args = self.args()
        args.rid = "linux-x64"
        with self.assertRaisesRegex(RuntimeError, "Unsupported"):
            module.restore(args)
        self.assertFalse(self.sdk.exists())

    def test_traversal_rejected(self):
        self.valid()
        self.make_archive("saltsutils.native", [
            ("lib/cmake/SaltsUtils/SaltsUtilsConfig.cmake", b"utils"),
            ("../../escape.txt", b"malicious")
        ], self.utils)
        with self.assertRaisesRegex(RuntimeError, "Unsafe package path"):
            module.restore(self.args())
        self.assertFalse(self.sdk.exists())

    def test_duplicate_entry_rejected(self):
        self.valid()
        self.make_archive("saltsutils.native", [
            ("lib/cmake/SaltsUtils/SaltsUtilsConfig.cmake", b"one"),
            ("lib/cmake/SaltsUtils/SaltsUtilsConfig.cmake", b"two")
        ], self.utils)
        with self.assertRaisesRegex(RuntimeError, "Duplicate SDK entry"):
            module.restore(self.args())
        self.assertFalse(self.sdk.exists())

    def test_collision_rejected(self):
        self.valid()
        self.make_archive("saltsutils.native", [
            ("include/vstr.h", b"conflicting"),
            ("lib/cmake/SaltsUtils/SaltsUtilsConfig.cmake", b"utils")
        ], self.utils)
        with self.assertRaisesRegex(RuntimeError, "conflict"):
            module.restore(self.args())
        self.assertFalse(self.sdk.exists())

    def test_symlink_rejected(self):
        self.valid()
        self.make_archive("saltsutils.native", [
            ("lib/cmake/SaltsUtils/SaltsUtilsConfig.cmake", b"utils"),
            ("include/symlink", b"../vstr.h", stat.S_IFLNK | 0o777)
        ], self.utils)
        with self.assertRaisesRegex(RuntimeError, "symlink"):
            module.restore(self.args())
        self.assertFalse(self.sdk.exists())


if __name__ == "__main__":
    unittest.main()

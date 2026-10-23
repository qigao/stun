#!/usr/bin/env python3
"""Regression tests for the bounded Tango Salts SDK restorer."""

import hashlib
import pathlib
import subprocess
import sys
import tempfile
import zipfile


SCRIPT = pathlib.Path(__file__).parents[1] / "restore-tango-salts-sdk.py"
RELEASE = "v2.3.0-rc.17"
SOURCE_SHA = "1" * 40


def write_package(path, malicious=False):
    root = "sdk/linux-x64/"
    with zipfile.ZipFile(path, "w") as archive:
        archive.writestr(root + "salts-sdk-manifest.txt",
                         "version=2.3.0-rc.17\ncommit=" + SOURCE_SHA +
                         "\nprofile=release\n")
        archive.writestr(root + "include/vstr.h", "/* vstr */\n")
        archive.writestr(root + "include/unicode/salts_unicode.h",
                         "/* unicode */\n")
        archive.writestr(root + "lib/cmake/Salts/SaltsConfig.cmake", "\n")
        archive.writestr(root + "lib/libsalts_unicode.a", "archive")
        if malicious:
            archive.writestr(root + "../../escape", "bad")


def invoke(package, checksums, sdk):
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--package", str(package),
         "--checksums", str(checksums), "--sdk", str(sdk),
         "--rid", "linux-x64", "--release", RELEASE,
         "--source-sha", SOURCE_SHA],
        check=False, capture_output=True, text=True)


def main():
    with tempfile.TemporaryDirectory() as temporary:
        root = pathlib.Path(temporary)
        package = root / "Salts.Native.2.3.0-rc.17.nupkg"
        checksums = root / "SHA256SUMS"
        write_package(package)
        checksums.write_text(hashlib.sha256(package.read_bytes()).hexdigest() +
                             "  dist/" + package.name + "\n", encoding="utf-8")
        result = invoke(package, checksums, root / "sdk")
        assert result.returncode == 0, result.stderr
        assert (root / "sdk/tango-salts-sdk-provenance.json").is_file()

        bad_sums = root / "bad-sums"
        bad_sums.write_text("0" * 64 + "  " + package.name + "\n",
                            encoding="utf-8")
        assert invoke(package, bad_sums, root / "bad-digest").returncode != 0

        malicious = root / "malicious.nupkg"
        write_package(malicious, malicious=True)
        malicious_sums = root / "malicious-sums"
        malicious_sums.write_text(
            hashlib.sha256(malicious.read_bytes()).hexdigest() +
            "  " + malicious.name + "\n", encoding="utf-8")
        result = invoke(malicious, malicious_sums, root / "malicious-sdk")
        assert result.returncode != 0
    print("Tango Salts SDK restorer tests passed")


if __name__ == "__main__":
    main()

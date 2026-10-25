#!/usr/bin/env python3
"""Restore two official native SDK packages for a single cross-platform FlexUI/Tango test.

Fail closed on invalid digests, archive traversal, duplicate/conflicting
package-owned paths, symlinks, missing exported targets and wrong RID.
No NuGet/Mono runtime or source dependency fallback.
"""
import argparse
import hashlib
import json
import pathlib
import re
import stat
import tempfile
import zipfile


def require(ok, why):
    if not ok:
        raise RuntimeError(why)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def checked_archive(package, checksum_file):
    require(package.is_file() and checksum_file.is_file(),
            f"Missing published package or SHA256SUMS: {package}")
    matches = []
    for line in checksum_file.read_text(encoding="utf-8-sig").splitlines():
        columns = line.strip().split()
        if len(columns) >= 2 and pathlib.PurePosixPath(
                columns[-1].lstrip("*")).name.lower() == package.name.lower():
            matches.append(columns[0].lower())
    require(len(matches) == 1 and
            re.fullmatch(r"[0-9a-f]{64}", matches[0]) is not None,
            f"Missing or ambiguous SHA256SUMS entry: {package.name}")
    actual = sha256(package)
    require(actual == matches[0],
            f"Official digest mismatch: {package.name}")
    return actual


def extract(package, dest, rid):
    prefix = pathlib.PurePosixPath("sdk") / rid
    seen = set()
    copied = 0
    with zipfile.ZipFile(package) as archive:
        for info in sorted(archive.infolist(), key=lambda value: value.filename):
            source = pathlib.PurePosixPath(info.filename)
            if source == prefix or prefix not in source.parents:
                continue
            relative = source.relative_to(prefix)
            # ZIP paths are POSIX, but pathlib on Windows interprets a
            # single backslash or a drive colon as path syntax. Reject both
            # before touching the filesystem, not only ".." POSIX parts.
            unsafe = (not relative.parts or relative.is_absolute() or
                      any(part in ("", ".", "..") or
                          chr(92) in part or chr(0) in part or ":" in part or
                          part.endswith((" ", ".")) for part in relative.parts))
            require(not unsafe, f"Unsafe package path: {info.filename}")
            require(relative not in seen,
                    f"Duplicate SDK entry: {info.filename}")
            seen.add(relative)
            mode = info.external_attr >> 16
            require(not stat.S_ISLNK(mode),
                    f"SDK symlink not permitted: {info.filename}")
            target = dest.joinpath(*relative.parts)
            if info.is_dir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            data = archive.read(info)
            target.parent.mkdir(parents=True, exist_ok=True)
            require(not target.is_symlink(),
                    f"SDK symlink collision: {target}")
            if target.exists():
                require(target.is_file() and target.read_bytes() == data,
                        f"Two providers conflict on SDK path: {relative}")
            else:
                target.write_bytes(data)
                copied += 1
    require(copied != 0, f"No sdk/{rid} files in {package.name}")
    return copied


def restore(args):
    require(re.fullmatch(r"(macos-arm64|windows-x64)", args.rid) is not None,
            f"Unsupported cross-platform RID: {args.rid}")
    for release in (args.salts_release, args.salts_utils_release):
        require(re.fullmatch(r"v?[0-9]+[.][0-9]+[.][0-9]+"
                             r"(?:-[0-9a-zA-Z.]+)?", release) is not None,
                f"Invalid release: {release}")
    require(not args.sdk.exists(), "SDK output already exists")
    args.sdk.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="stun-sdks-", dir=args.sdk.parent) as temporary:
        stage = pathlib.Path(temporary)
        packages = [
            ("salts.native", args.salts_release, args.salts_checksums),
            ("saltsutils.native", args.salts_utils_release, args.salts_utils_checksums),
        ]
        manifest = {"kind": "official-published-cross-platform-native-sdks",
                    "rid": args.rid, "packages": {}}
        for name, release, checksums in packages:
            version = release.removeprefix("v").lower()
            filename = f"{name}.{version}.nupkg"
            archive = args.packages / name / version / filename
            digest = checked_archive(archive, checksums)
            copied = extract(archive, stage, args.rid)
            manifest["packages"][name] = {
                "release": release, "sha256": digest, "copied": copied
            }
        for required in (
            "include/vstr.h",
            "include/unicode/salts_unicode.h",
            "lib/cmake/Salts/SaltsConfig.cmake",
            "lib/cmake/SaltsUtils/SaltsUtilsConfig.cmake",
        ):
            require((stage / required).is_file(),
                    f"Published SDK missing {required}")
        (stage / "flexui-tango-sdk-provenance.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8")
        # The output remains absent on all validation failures.
        stage.rename(args.sdk)
        print(json.dumps(manifest, sort_keys=True))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--packages", required=True, type=pathlib.Path)
    parser.add_argument("--sdk", required=True, type=pathlib.Path)
    parser.add_argument("--salts-release", required=True)
    parser.add_argument("--salts-utils-release", required=True)
    parser.add_argument("--salts-checksums", required=True, type=pathlib.Path)
    parser.add_argument("--salts-utils-checksums", required=True, type=pathlib.Path)
    parser.add_argument("--rid", required=True)
    restore(parser.parse_args())


if __name__ == "__main__":
    main()

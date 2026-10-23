#!/usr/bin/env python3
"""Verify and restore one published Salts SDK RID for Tango CI."""

import argparse
import hashlib
import json
import pathlib
import re
import stat
import zipfile


def require(condition, reason):
    if not condition:
        raise RuntimeError(reason)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def official_digest(package, checksums):
    matches = []
    for line in checksums.read_text(encoding="utf-8").splitlines():
        fields = line.strip().split()
        if (len(fields) >= 2 and
                pathlib.PurePosixPath(fields[-1].lstrip("*")).name.lower() ==
                package.name.lower()):
            matches.append(fields[0].lower())
    require(len(matches) == 1,
            f"Missing or ambiguous official SHA256 for {package.name}")
    observed = digest(package)
    require(re.fullmatch(r"[0-9a-f]{64}", matches[0]) is not None,
            "Malformed official SHA256")
    require(observed == matches[0], f"SHA256 mismatch for {package.name}")
    return observed


def extract_rid(package, sdk, rid):
    root = pathlib.PurePosixPath("sdk") / rid
    seen = set()
    copied = 0
    with zipfile.ZipFile(package) as archive:
        for member in sorted(archive.infolist(), key=lambda item: item.filename):
            source = pathlib.PurePosixPath(member.filename)
            if source == root or root not in source.parents:
                continue
            relative = source.relative_to(root)
            require(relative.parts and ".." not in relative.parts,
                    f"Unsafe SDK archive path: {member.filename}")
            require(relative not in seen,
                    f"Duplicate SDK archive entry: {member.filename}")
            seen.add(relative)
            mode = member.external_attr >> 16
            require(not stat.S_ISLNK(mode),
                    f"Unsupported SDK symlink: {member.filename}")
            destination = sdk.joinpath(*relative.parts)
            if member.is_dir():
                destination.mkdir(parents=True, exist_ok=True)
                continue
            destination.parent.mkdir(parents=True, exist_ok=True)
            require(not destination.exists(),
                    f"Duplicate SDK destination: {destination}")
            destination.write_bytes(archive.read(member))
            copied += 1
    require(copied != 0, f"Published package has no sdk/{rid} payload")


def validate_sdk(sdk, release, source_sha, rid, package_sha):
    manifest_path = sdk / "salts-sdk-manifest.txt"
    require(manifest_path.is_file(), "SDK manifest is missing")
    manifest = dict(
        line.split("=", 1)
        for line in manifest_path.read_text(encoding="utf-8-sig").splitlines()
        if "=" in line
    )
    require(manifest.get("version") == release.removeprefix("v"),
            "SDK manifest version mismatch")
    require(manifest.get("commit") == source_sha,
            "SDK manifest commit mismatch")
    require(manifest.get("profile") == "release",
            "SDK is not a release profile")
    for required in (
        "include/vstr.h",
        "include/unicode/salts_unicode.h",
        "lib/cmake/Salts/SaltsConfig.cmake",
    ):
        require((sdk / required).is_file(), f"SDK missing {required}")
    require(any(path.is_file() for path in (sdk / "lib").glob("*salts_unicode*")),
            "SDK missing the Unicode library")
    evidence = {
        "kind": "official-published-salts-sdk",
        "rid": rid,
        "release": release,
        "source_sha": source_sha,
        "package_sha256": package_sha,
    }
    (sdk / "tango-salts-sdk-provenance.json").write_text(
        json.dumps(evidence, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--package", required=True, type=pathlib.Path)
    parser.add_argument("--checksums", required=True, type=pathlib.Path)
    parser.add_argument("--sdk", required=True, type=pathlib.Path)
    parser.add_argument("--rid", required=True)
    parser.add_argument("--release", required=True)
    parser.add_argument("--source-sha", required=True)
    args = parser.parse_args()

    require(re.fullmatch(r"v[0-9]+[.][0-9]+[.][0-9]+(?:-[0-9A-Za-z.]+)?",
                         args.release) is not None,
            "Invalid release tag")
    require(re.fullmatch(r"[0-9a-fA-F]{40}", args.source_sha) is not None,
            "Invalid release source SHA")
    require(re.fullmatch(r"(?:linux|macos|windows)-(?:x64|arm64)", args.rid)
            is not None, "Unsupported SDK RID")
    args.sdk.mkdir(parents=True, exist_ok=True)
    require(not any(args.sdk.iterdir()), "SDK destination must be empty")

    package_sha = official_digest(args.package, args.checksums)
    extract_rid(args.package, args.sdk, args.rid)
    validate_sdk(args.sdk, args.release, args.source_sha, args.rid, package_sha)


if __name__ == "__main__":
    main()

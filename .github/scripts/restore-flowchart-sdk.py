#!/usr/bin/env python3
"""Restore verified published Salts native SDKs for isolated Flowchart golden CI.

This script accepts immutable, already-downloaded NuGet packages and the
publishing repositories' official SHA256SUMS files. No source-library fallback,
unspecified version, partial install, or unverified alias is permitted.
"""
import argparse
import hashlib
import json
import pathlib
import re
import shutil
import subprocess
import zipfile


def require(condition, reason):
    if not condition:
        raise RuntimeError(reason)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_official_digest(archive, sums):
    require(archive.is_file(), f"Missing exact NuGet archive: {archive}")
    rows = []
    for line in sums.read_text(encoding="utf-8").splitlines():
        tokens = line.strip().split()
        if len(tokens) >= 2 and pathlib.Path(tokens[-1].lstrip("*")).name.lower() == archive.name.lower():
            rows.append(tokens[0].lower())
    require(len(rows) == 1, f"Missing/ambiguous official SHA256 for {archive.name}")
    actual = digest(archive)
    require(actual == rows[0], f"Official SHA256 mismatch for {archive.name}")
    return actual


def extract_sdk_payload(archive, sdk):
    copied = 0
    root = pathlib.PurePosixPath("sdk/linux-x64")
    with zipfile.ZipFile(archive) as package:
        for member in package.infolist():
            rel = pathlib.PurePosixPath(member.filename)
            if rel == root:
                continue
            if root not in rel.parents:
                continue
            tail = rel.relative_to(root)
            require(tail.parts and ".." not in tail.parts, "Unsafe SDK archive path")
            dst = sdk.joinpath(*tail.parts)
            if member.is_dir():
                dst.mkdir(parents=True, exist_ok=True)
                continue
            require(not dst.is_symlink(), f"Unexpected pre-existing SDK link: {dst}")
            data = package.read(member)
            dst.parent.mkdir(parents=True, exist_ok=True)
            if dst.exists():
                require(dst.is_file() and dst.read_bytes() == data,
                        f"Conflicting package-owned SDK object: {dst}")
            else:
                dst.write_bytes(data)
                copied += 1
    require(copied != 0, f"No installed linux-x64 SDK payload in {archive.name}")
    return copied


def canonicalize_core(sdk, salts_version):
    lib = sdk / "lib"
    core = lib / ("libsalts.so." + salts_version.split("-", 1)[0])
    require(core.is_file() and not core.is_symlink(), f"Missing canonical Core DSO: {core}")
    metadata = subprocess.check_output(["readelf", "-d", str(core)], text=True)
    sonames = re.findall(r"\\(SONAME\\).*?\\[([^\\]]+)\\]", metadata)
    require(len(sonames) == 1 and re.fullmatch(r"libsalts[.]so[.][0-9]+(?:[.][0-9]+)*", sonames[0]),
            f"Unexpected Core ELF SONAME: {sonames}")
    require(core.name.startswith(sonames[0] + "."), "Core SONAME/release mismatch")
    core_hash = digest(core)
    for alias in (sonames[0], "libsalts.so"):
        candidate = lib / alias
        require(candidate.is_file() and digest(candidate) == core_hash,
                f"Missing or conflicting published Core alias: {candidate}")
        if candidate.is_symlink():
            require(candidate.resolve(strict=True) == core.resolve(strict=True),
                    f"Core symlink points to different provider: {candidate}")
        else:
            candidate.unlink()
            candidate.symlink_to(core.name)
        require(candidate.resolve(strict=True) == core.resolve(strict=True),
                f"Core provider identity mismatch: {candidate}")
    return {"canonical": core.name, "soname": sonames[0], "sha256": core_hash}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--packages", required=True, type=pathlib.Path)
    parser.add_argument("--sdk", required=True, type=pathlib.Path)
    parser.add_argument("--salts-release", required=True)
    parser.add_argument("--salts-utils-release", required=True)
    parser.add_argument("--salts-checksums", required=True, type=pathlib.Path)
    parser.add_argument("--salts-utils-checksums", required=True, type=pathlib.Path)
    args = parser.parse_args()

    args.sdk.mkdir(parents=True, exist_ok=True)
    require(not any(args.sdk.iterdir()), "Installed SDK destination must be empty")
    manifest = {"kind": "official-published-native-sdk", "rid": "linux-x64", "packages": {}}
    packages = (
        ("salts.native", args.salts_release, args.salts_checksums),
        ("saltsutils.native", args.salts_utils_release, args.salts_utils_checksums),
    )
    for name, release, sums in packages:
        require(re.match(r"^v?[0-9]+[.][0-9]+[.][0-9]+(?:-[0-9A-Za-z.]+)?$", release),
                f"Invalid pinned SDK release: {release}")
        version = release.removeprefix("v")
        archive = args.packages / name / version / f"{name}.{version}.nupkg"
        sha256 = verify_official_digest(archive, sums)
        copied = extract_sdk_payload(archive, args.sdk)
        manifest["packages"][name] = {"release": release, "sha256": sha256, "copied": copied}

    for required in (
        "include/json_parser.h",
        "include/tinytest.h",
        "lib/libjson_parser.a",
        "lib/libtinytest.a",
        "lib/cmake/Salts/SaltsConfig.cmake",
        "lib/cmake/SaltsUtils/SaltsUtilsConfig.cmake",
    ):
        require((args.sdk / required).is_file(), f"Published native SDK missing {required}")

    manifest["core"] = canonicalize_core(args.sdk, args.salts_release.removeprefix("v"))
    evidence = args.sdk / "flowchart-sdk-provenance.json"
    evidence.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print("Verified published SDKs: " + json.dumps(manifest, sort_keys=True))


if __name__ == "__main__":
    main()

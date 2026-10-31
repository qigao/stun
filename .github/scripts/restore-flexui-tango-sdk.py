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
import os
import shutil
import stat
import subprocess
import sys
import tempfile
from types import SimpleNamespace
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


def restore_qualified(args):
    """Consume an exact successful producer run, without a released-Utils fallback."""
    root = args.qualified_sdk
    manifest = json.loads((root / "sdk-provenance.json").read_text(encoding="utf-8-sig"))
    for key, expected in (
        ("repository", "qigao/salts-utils"), ("source_sha", args.source_sha),
        ("run_id", args.run_id), ("rid", args.rid), ("configuration", "Release"),
    ):
        require(expected is not None and manifest.get(key) == expected,
                f"Qualified SDK provenance mismatch: {key}")
    require(re.fullmatch(r"[0-9a-f]{40}", args.source_sha) is not None,
            "Invalid qualified source SHA")
    require(args.rid in ("linux-x64", "macos-arm64", "windows-x64"),
            "Unsupported qualified SDK RID")
    require(not args.sdk.exists(), "SDK output already exists")
    args.sdk.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="stun-qualified-", dir=args.sdk.parent) as temporary:
        stage = pathlib.Path(temporary)
        # Preserve the same unified installed-prefix contract used by existing
        # Stun native builds. Every provider comes from the pinned producer run.
        for provider in ("salts", "utils", "crypto"):
            source = root / provider
            require(source.is_dir() and not source.is_symlink(),
                    f"Missing qualified SDK provider: {provider}")
            for entry in sorted(source.rglob("*")):
                relative = entry.relative_to(source)
                require(not entry.is_symlink(), f"Qualified SDK symlink: {relative}")
                target = stage / relative
                if entry.is_dir():
                    target.mkdir(parents=True, exist_ok=True)
                else:
                    require(entry.is_file(), f"Invalid qualified SDK object: {relative}")
                    target.parent.mkdir(parents=True, exist_ok=True)
                    if target.exists():
                        require(target.is_file() and target.read_bytes() == entry.read_bytes(),
                                f"Qualified SDK provider conflict: {relative}")
                    else:
                        shutil.copy2(entry, target)
        for required in (
            "include/salts/markdown.h", "include/salts/markdown_html.h",
            "lib/cmake/Salts/SaltsConfig.cmake",
            "lib/cmake/SaltsUtils/SaltsUtilsConfig.cmake",
            "lib/cmake/Crypto/CryptoConfig.cmake",
        ):
            require((stage / required).is_file(), f"Qualified SDK missing {required}")
        archives = (("markdown_parser.lib", "markdown_html.lib") if args.rid == "windows-x64"
                    else ("libmarkdown_parser.a", "libmarkdown_html.a"))
        for archive in archives:
            require((stage / "lib" / archive).is_file(), f"Qualified SDK missing {archive}")
        (stage / "sdk-provenance.json").write_text(
            json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        stage.rename(args.sdk)
    if args.export_env:
        with open(os.environ["GITHUB_ENV"], "a", encoding="utf-8") as env:
            for name in ("SALTS_ROOT", "SALTS_UTILS_ROOT", "CRYPTO_ROOT", "CMAKE_PREFIX_PATH"):
                env.write(f"{name}={args.sdk.resolve()}\n")
    print("Qualified installed SDK: " + json.dumps(manifest, sort_keys=True))


def export_source_sdk(args):
    """Admit the same installed source owner on cache hits and fresh builds."""
    manifest = json.loads((args.sdk / "sdk-provenance.json").read_text(encoding="utf-8-sig"))
    for key, expected in (("repository", "qigao/salts-utils"),
                          ("source_sha", args.source_sha), ("rid", args.rid),
                          ("configuration", "Release")):
        require(manifest.get(key) == expected, f"Source SDK provenance mismatch: {key}")
    if args.export_env:
        with open(os.environ["GITHUB_ENV"], "a", encoding="utf-8") as env:
            for name in ("SALTS_ROOT", "SALTS_UTILS_ROOT", "CRYPTO_ROOT", "CMAKE_PREFIX_PATH"):
                env.write(f"{name}={args.sdk.resolve()}\n")
    print("Installed source SDK: " + json.dumps(manifest, sort_keys=True))


def build_source_sdk(args):
    """Build the real producer root graph when the installed SDK cache is absent.

    Cache eviction only changes build cost. The recipe and immutable source
    revisions remain in Git, independent of any Actions run or artifact ID.
    """
    require(args.source_sha is not None and
            re.fullmatch(r"[0-9a-f]{40}", args.source_sha) is not None,
            "Invalid source SDK SHA")
    profiles = {
        "linux-x64": ("ci-linux-release-user", "x64-linux"),
        "windows-x64": ("ci-win-release-user", "qigao-x64-windows-v145"),
        "macos-arm64": ("ci-macos-release-user", "arm64-osx"),
    }
    require(args.rid in profiles, "Unsupported source SDK RID")
    require(not args.sdk.is_symlink(), "Source SDK output must not be a symlink")
    if args.sdk.exists():
        export_source_sdk(args)
        return
    root = pathlib.Path(os.environ["RUNNER_TEMP"]) / "stun-saltsutils-source"
    root.mkdir()
    child = dict(os.environ)
    env_path, path_path = root / "producer.env", root / "producer.path"
    env_path.touch()
    path_path.touch()
    preset, triplet = profiles[args.rid]
    child.update(GITHUB_WORKSPACE=str(root), GITHUB_ENV=str(env_path),
                 GITHUB_PATH=str(path_path), QIGAO_SDK_RID=args.rid,
                 QIGAO_HOST_RID=args.rid, QIGAO_TARGET_TRIPLET=triplet,
                 QIGAO_HOST_TRIPLET=triplet,
                 QIGAO_NUGET_PACKAGES=str(root / "stage/nuget"))
    child["GITHUB_TOKEN"] = child.get("GITHUB_TOKEN") or child["GH_TOKEN"]

    def run(*command, cwd=root):
        print("+", *map(str, command), flush=True)
        subprocess.run(list(map(str, command)), cwd=cwd, env=child, check=True)

    def absorb_exports():
        for line in env_path.read_text(encoding="utf-8-sig").splitlines():
            key, value = line.split("=", 1)
            child[key] = value
        additions = path_path.read_text(encoding="utf-8-sig").splitlines()
        if additions:
            child["PATH"] = os.pathsep.join(additions + [child["PATH"]])
        env_path.write_text("")
        path_path.write_text("")

    def checkout(repository, revision, destination):
        destination.mkdir(parents=True, exist_ok=True)
        run("git", "init", str(destination))
        run("git", "-C", destination, "remote", "add", "origin",
            f"https://github.com/{repository}.git")
        run("git", "-C", destination, "fetch", "--depth=1", "origin", revision)
        run("git", "-C", destination, "checkout", "--detach", "FETCH_HEAD")

    checkout("qigao/salts-utils", args.source_sha, root)
    # Same cache contract and Crypto inputs as the accepted producer profile.
    contract = root / "build/ci-vcpkg-contract"
    checkout("qigao/vcpkg-cache", "fafb1efe5ac2440bfd7a08e493988d0459d89e51", contract)
    checkout("qigao/crypto", "a99f29a569af14b865c9e713360aa81d39a04bf0", root / "_ci_crypto")
    checkout("guanzhi/GmSSL", "7c9f02904ef33e59c87b4f16621cc8fd434e7579", root / "_ci_gmssl")
    if args.rid == "linux-x64":
        run("sudo", "apt-get", "update", "-qq")
        run("sudo", "apt-get", "install", "-y", "--no-install-recommends",
            "ninja-build", "pkg-config", "nasm", "libudev-dev",
            "libpipewire-0.3-dev", "libx11-dev", "libxext-dev")
    elif args.rid == "macos-arm64":
        run("brew", "install", "ninja", "nasm", "autoconf", "automake",
            "autoconf-archive", "libtool", "gcc@15")
        for key, tool in (("QIGAO_APPLE_CLANG", "clang"), ("QIGAO_APPLE_CLANGXX", "clang++")):
            child[key] = subprocess.check_output(["xcrun", "--find", tool], text=True).strip()
    else:
        run("choco", "install", "nasm", "ninja", "-y", "--no-progress")
    run(sys.executable, "cmake/ci/setup_native_ci.py", contract)
    absorb_exports()
    child["QIGAO_VCPKG_ROOT"] = child["VCPKG_ROOT"]
    run(sys.executable, "cmake/ci/github_packages.py", "native-sdk", "--packages",
        child["QIGAO_NUGET_PACKAGES"], "--salts-rid", args.rid, "--host-rid", args.rid)
    absorb_exports()
    run(sys.executable, "cmake/ci/install_static_crypto_sdk.py")
    absorb_exports()
    build = root / "build/stun-installed-sdk"
    providers = root / "installed-providers"
    utils = providers / "utils"
    # Configure the official root preset and build its complete graph. Parser
    # conformance is owned by upstream CI; downstream runs its real UI tests.
    run("cmake", "--preset", preset, "-B", build, "-DBUILD_TESTS=OFF",
        "-DBUILD_BENCHMARKS=OFF", "-DBUILD_EXAMPLES=OFF")
    run("cmake", "--build", build, "--config", "Release", "--parallel", "2")
    run("cmake", "--install", build, "--config", "Release", "--prefix", utils)
    for owner, source in (("salts", child["SALTS_ROOT"]), ("crypto", child["CRYPTO_ROOT"])):
        shutil.copytree(source, providers / owner)
    # Materialize CMake's library aliases as the same ordinary files used by
    # the already-qualified installed Actions SDK, retaining executable bits.
    flattened = root / "installed-sdk"
    shutil.copytree(utils, flattened)
    shutil.rmtree(utils)
    flattened.rename(utils)
    run_id = os.environ.get("GITHUB_RUN_ID", "local-source-build")
    manifest = dict(repository="qigao/salts-utils", source_sha=args.source_sha,
                    rid=args.rid, configuration="Release", run_id=run_id,
                    provider="immutable-source-root-preset")
    (providers / "sdk-provenance.json").write_text(json.dumps(manifest), encoding="utf-8")
    restore_qualified(SimpleNamespace(qualified_sdk=providers, sdk=args.sdk,
        source_sha=args.source_sha, rid=args.rid, run_id=run_id, export_env=False))
    export_source_sdk(args)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--packages", type=pathlib.Path)
    parser.add_argument("--sdk", required=True, type=pathlib.Path)
    parser.add_argument("--salts-release")
    parser.add_argument("--salts-utils-release")
    parser.add_argument("--salts-checksums", type=pathlib.Path)
    parser.add_argument("--salts-utils-checksums", type=pathlib.Path)
    parser.add_argument("--rid", required=True)
    parser.add_argument("--qualified-sdk", type=pathlib.Path)
    parser.add_argument("--source-sha")
    parser.add_argument("--run-id")
    parser.add_argument("--export-env", action="store_true")
    parser.add_argument("--build-source-sdk", action="store_true")
    args = parser.parse_args()
    if args.build_source_sdk:
        build_source_sdk(args)
    elif args.qualified_sdk is not None:
        restore_qualified(args)
    else:
        require(all(getattr(args, name) is not None for name in (
            "packages", "salts_release", "salts_utils_release", "salts_checksums",
            "salts_utils_checksums")), "Published SDK restore arguments are required")
        restore(args)


if __name__ == "__main__":
    main()

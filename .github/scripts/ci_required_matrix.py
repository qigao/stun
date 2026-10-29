#!/usr/bin/env python3
"""Trusted, fail-closed source-delta -> actual GitHub job requirements.

Use only from the repository's DEFAULT BRANCH (never the PR checkout).
Canonical workflow pull_request.paths drive routing; exact job names are
pinned here. An unmapped source file is an admission error, not "green".
"""
import fnmatch
from pathlib import Path

WORKFLOWS = {
    "flexui-tango-headless.yml": (
        "FlexUI/Tango headless / macos-latest",
        "FlexUI/Tango headless / windows-latest",
    ),
    "gcanvas-android-egl.yml": ("android-egl-compile",),
    "gcanvas-android-runtime.yml": ("android-surface-lifecycle",),
    "gcanvas-gles-runtime.yml": ("gles-runtime",),
    "gcanvas-package-contract.yml": ("gles-installed-consumer", "android-default-cmake"),
    "gcanvas-shader-tools-cache.yml": ("Linux shared shader-tool cache",),
    "graphlayout-native.yml": (
        "ubuntu-24.04 / gcc", "ubuntu-24.04 / clang",
        "windows-2022 / msvc", "macos-15 / appleclang",
    ),
    "markdown-highlighter-loader.yml": (
        "DSO loader / ubuntu-24.04", "DSO loader / macos-15",
        "DSO loader / windows-2022", "Highlighter ownership / Linux TSan",
    ),
    "tango-core.yml": (
        "Native C / ubuntu-latest", "Native C / macos-latest",
        "Native C / windows-latest",
    ),
    "ui-core-c11.yml": (
        "c11-headers", "C11 SDK compile / macos-15",
        "C11 SDK compile / windows-2022",
        "C11 native runtime / Windows x64 released Salts",
        "C11 native runtime / macOS ARM64 released Salts",
    ),
}
PUBLIC = ".github/workflows/public-dependency-contract.yml"
RUNTIME = ".github/workflows/runtime-conformance.yml"
PARSER = "Repository sole C11 parser contract"
NATIVE = "Linux / focused installed-SDK ASan/UBSan"

# These existing subsystems have the generic native Runtime Conformance gate
# even if there is no separate focused workflow for a particular source file.
RUNTIME_ROOTS = (
    "flex/", "flexUI/", "ui_core/", "tango/", "charts/", "shared/gcanvas/",
    "shared/layout/", "shared/markdown/", "cmake/", "vendor/", ".ci/",
    "tools/lemon/", "tools/md/", "tools/gcanvas-shaderc/",
)
RUNTIME_FILES = {"CMakeLists.txt", "vcpkg.json", "vcpkg-configuration.json"}
# CI governance scripts are independently exercised by the unfiltered
# public-dependency parser job. Source-level changes also require Runtime.
CI_INFRA_ROOTS = (".github/workflows/", ".github/actions/", ".github/scripts/")
# Only *unambiguously* non-executable documentation qualifies for the
# documentation-only gate. A source-owned *.md fixture may drive renderer,
# parser, GraphLayout or Tango tests; extension alone is NOT admission proof.
DOC_ROOTS = ("docs/", "documentation/")
DOC_FILES = {
    "LICENSE", "LICENSE.txt", "NOTICE", "README", ".editorconfig",
    ".github/CI_SCOPE.md", ".github/CCACHE.md",
}
DOC_SUFFIXES = (".md", ".rst", ".adoc", ".txt", ".pdf", ".png",
                ".jpg", ".jpeg", ".webp", ".svg", ".gif")


def is_doc(path):
    if (not isinstance(path, str) or not path or path.startswith("/")
            or "\\" in path or "\x00" in path or ".." in path.split("/")):
        return False
    if path in DOC_FILES:
        return True
    if path.startswith(DOC_ROOTS):
        return path.endswith(DOC_SUFFIXES)
    # Root-level documentation, not *.md inside source/test modules.
    return "/" not in path and path.endswith((".md", ".rst", ".adoc"))


def only_documentation_paths_z(filename):
    """Check NUL-delimited exact git diff paths, failing closed on ambiguity."""
    try:
        value = Path(filename).read_bytes()
        if not value or not value.endswith(b"\x00"):
            return False
        names = [segment.decode("utf-8") for segment in value[:-1].split(b"\x00")]
    except (OSError, UnicodeDecodeError):
        return False
    return bool(names) and all(is_doc(name) for name in names)


def known_source(path):
    return (path.startswith(RUNTIME_ROOTS) or
            path.startswith(CI_INFRA_ROOTS) or path in RUNTIME_FILES)


def pr_path_filters(contents):
    """Extract only top-level pull_request.paths, fail closed if malformed."""
    lines = contents.splitlines()
    pull_index = next(
        (i for i, line in enumerate(lines) if line == "  pull_request:"), None
    )
    if pull_index is None:
        raise ValueError("workflow lacks pull_request event")
    end = next((i for i in range(pull_index + 1, len(lines))
                if lines[i] and not lines[i].startswith(" ")), len(lines))
    # Top-level event siblings use two spaces, not zero.
    for i in range(pull_index + 1, len(lines)):
        if lines[i].startswith("  ") and not lines[i].startswith("    "):
            end = min(end, i)
            break
    start = next((i for i in range(pull_index + 1, end)
                  if lines[i] == "    paths:"), None)
    if start is None:
        raise ValueError("focused workflow has no pull_request.paths")
    patterns = []
    for line in lines[start + 1:end]:
        if not line.startswith("      - "):
            if line.strip():
                break
            continue
        pattern = line.strip()[2:].strip().strip("'\"")
        if not pattern or pattern in ("*", "**") or pattern.startswith("!"):
            raise ValueError("ambiguous or negative workflow path filter")
        patterns.append(pattern)
    if not patterns:
        raise ValueError("empty workflow PR path filter")
    return patterns


def workflow_requirements(changed_paths, workflow_sources):
    """Select only jobs from trusted, real triggered workflows."""
    if not isinstance(changed_paths, list) or not changed_paths:
        raise ValueError("PR has no changed file evidence")
    result = {PUBLIC: [PARSER]}
    changed = set()
    for value in changed_paths:
        if (not isinstance(value, str) or not value
                or value.startswith("/") or "\\" in value
                or "\x00" in value or ".." in value.split("/")):
            raise ValueError("untrusted PR changed file path")
        changed.add(value)

    non_docs = [name for name in changed if not is_doc(name)]
    unknown = sorted(name for name in non_docs if not known_source(name))
    if unknown:
        raise ValueError("unmapped source paths: " + ", ".join(unknown[:10]))
    if non_docs:
        result[RUNTIME] = [NATIVE]

    for filename, names in WORKFLOWS.items():
        source = workflow_sources.get(filename)
        if not isinstance(source, str):
            raise ValueError("missing trusted workflow: " + filename)
        patterns = pr_path_filters(source)
        if any(fnmatch.fnmatchcase(path, pattern)
               for path in changed for pattern in patterns):
            result[".github/workflows/" + filename] = list(names)
    return result


def load_workflow_requirements(paths, root):
    directory = Path(root) / ".github/workflows"
    sources = {name: (directory / name).read_text(encoding="utf-8")
               for name in WORKFLOWS}
    return workflow_requirements(paths, sources)


def pr_changed_paths(get, number, fetch_pages):
    """Read all PR files and both sides of renames. No truncated success."""
    # The caller injects the existing bounded M0 pagination function.
    files = fetch_pages(get, f"/pulls/{number}/files", None)
    paths = []
    for item in files:
        if not isinstance(item, dict):
            raise ValueError("malformed PR file record")
        filename = item.get("filename")
        if not isinstance(filename, str):
            raise ValueError("missing PR filename")
        paths.append(filename)
        if item.get("status") == "renamed":
            previous = item.get("previous_filename")
            if not isinstance(previous, str):
                raise ValueError("missing renamed source path")
            paths.append(previous)
    return paths

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 3 or sys.argv[1] != "--doc-only-z":
        sys.exit(2)
    sys.exit(0 if only_documentation_paths_z(sys.argv[2]) else 1)

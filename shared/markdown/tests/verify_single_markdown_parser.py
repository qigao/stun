#!/usr/bin/env python3
"""Static fast gate for Stun #172: the repository has one Markdown grammar.

This supplements (never replaces) native C11 conformance and the real
MarkdownWidget runtime tests.  The retired tools/md tree may retain only the
immutable upstream CommonMark fixture and its explanatory README.
"""
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[3]


def require(ok: bool, reason: str) -> None:
    if not ok:
        raise SystemExit("MarkdownWidget canonical C11 contract: " + reason)


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8-sig")


def main() -> None:
    widget = read("flexUI/src/widgets/markdown_widget.cpp")
    widget_header = read("flexUI/include/flexUI/widgets/markdown_widget.h")
    widget_cmake = read("flexUI/CMakeLists.txt")
    tests_cmake = read("flexUI/tests/CMakeLists.txt")
    project_cmake = read("CMakeLists.txt")
    demo = read("flexUI/examples/markdown_demo.cpp")
    tools_cmake = read("tools/CMakeLists.txt")

    fixture_root = ROOT / "tools/md"
    retained = {
        "tools/md/README.md",
        "tools/md/test/spec.txt",
    }
    actual = {
        path.relative_to(ROOT).as_posix()
        for path in fixture_root.rglob("*")
        if path.is_file()
    }
    require(actual == retained,
            "retired tools/md contains parser/build files: " +
            ", ".join(sorted(actual - retained)))
    require(not re.search(r"add_subdirectory\s*\(\s*md\s*\)", tools_cmake),
            "retired tools/md build reintroduced")

    production_files = []
    for root in (ROOT / "flexUI", ROOT / "tango", ROOT / "shared"):
        production_files.extend(
            path for path in root.rglob("*")
            if path.is_file() and
            (path.name == "CMakeLists.txt" or
             path.suffix.lower() in {".c", ".cc", ".cpp", ".h", ".hpp"})
        )
    forbidden = re.compile(r"\bmd::parse\s*\(|\bmd_parser\b|"
                           r"tools/md/(?:include|src)")
    offenders = [
        path.relative_to(ROOT).as_posix()
        for path in production_files
        if forbidden.search(path.read_text(encoding="utf-8-sig"))
    ]
    require(not offenders,
            "legacy Markdown parser reference reintroduced: " +
            ", ".join(offenders))

    require(bool(re.search(r"^\s*#include <stun/markdown_c11\.h>",
                           widget, re.M)), "missing canonical C11 header")
    require(len(re.findall(r"\bstun_md_parse\s*\(", widget)) == 1,
            "expected exactly one C11 parser call in MarkdownWidget")
    require("stun_md_document_destroy" in widget,
            "missing explicit C11 document ownership")
    require("stun_md_document_view" in widget,
            "missing typed RichDoc projection")
    require(not re.search(r'^\s*#include\s*[<"](?:md|md4c)(?:[/\.]|>)',
                          widget, re.M), "legacy parser header reintroduced")
    require("vendor/md/include" not in widget_cmake,
            "legacy Markdown headers exported by FlexUI")
    require("vendor/md4c/src" not in tests_cmake,
            "legacy MD4C header path retained by Widget test")
    require(bool(re.search(r"target_link_libraries\s*\(\s*flexUI\s+"
                           r"PRIVATE\s+Stun::UICore\s*\)", project_cmake)),
            "canonical Stun::UICore missing from Widget runtime link")
    require("src/widgets/markdown_widget.cpp" in widget_cmake,
            "production widget source is missing")
    require("test_markdown_widget.cpp" in tests_cmake and
            "add_test(NAME test_markdown_widget" in tests_cmake,
            "real Widget ownership/runtime test is unregistered")
    require("MarkdownWidget" in widget_header and
            "stun_md_parse" in widget and "MD4C" not in demo,
            "public Widget/demo authority is inconsistent")
    print("PASS: repository-wide single C11 RichDoc parser boundary (#172)")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Assert one Markdown grammar owner: imported SaltsUtils, no Stun fallback."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]

def require(ok: bool, reason: str) -> None:
    if not ok:
        raise SystemExit("Stun canonical Markdown: " + reason)

def read(p: str) -> str:
    return (ROOT / p).read_text(encoding="utf-8-sig")

def main() -> None:
    lib = read("shared/markdown/CMakeLists.txt")
    adapter = read("shared/markdown/src/markdown_adapter.c")
    ui = read("shared/markdown/src/ui_core.c")
    header = read("shared/markdown/include/stun/ui_core.h")
    widget = read("flexUI/src/widgets/markdown_widget.cpp")
    tests = read("shared/markdown/tests/CMakeLists.txt")
    for name in ("markdown_c11.c", "markdown_entities.c",
                 "markdown_entities.h", "markdown_entities_table.inc",
                 "markdown_html.c"):
        require(not (ROOT / "shared/markdown/src" / name).exists(),
                "duplicate Markdown source: " + name)
    require(not (ROOT / "tools/md/test/spec.txt").exists(),
            "original CommonMark spec corpus must belong to SaltsUtils")
    allowed = {
        "CMakeLists.txt", "ui_core_test.c", "markdown_adapter_test.c",
        "markdown_tango_candidate_test.c", "verify_single_markdown_parser.py",
    }
    actual = {p.name for p in (ROOT / "shared/markdown/tests").iterdir()
              if p.is_file()}
    require(actual == allowed, "parser-only tests still in Stun: "
            + repr(sorted(actual - allowed)))
    for target in ("Salts::MarkdownParser", "Salts::MarkdownHtml"):
        require(target in lib, "missing imported " + target)
    require("src/markdown_adapter.c" in lib, "missing forwarder")
    require("src/markdown_c11.c" not in lib and
            "src/markdown_html.c" not in lib,
            "Stun Markdown grammar/renderer reintroduced")
    require("salts_md_parse(" in adapter and
            "salts_md_get_document_view(" in adapter,
            "public Stun C API must forward into canonical parser")
    require("salts_md_parse(" not in ui and
            "stun_md_parse(" not in ui, "UI runtime must not parse Markdown")
    require("#include <salts/markdown.h>" in header,
            "Markdown types must alias canonical RichDoc types")
    require("stun_md_parse(" in widget and
            "stun_md_document_destroy" in widget and
            "stun_md_document_view" in widget,
            "real MarkdownWidget not using Stun forwarding entrypoint")
    require("find_package(SaltsUtils CONFIG REQUIRED" in read("CMakeLists.txt"),
            "Stun must have exactly one installed SaltsUtils dependency")
    require("stun_markdown_adapter_test" in tests and
            "stun_markdown_tango_candidate_test" in tests,
            "Stun real consumer tests missing")
    require("test_markdown_widget" in read("flexUI/tests/CMakeLists.txt"),
            "real MarkdownWidget test missing")
    for folder in (ROOT / "shared/markdown", ROOT / "flexUI"):
        for f in folder.rglob("*"):
            if not f.is_file() or f.suffix not in (".h",".hpp",".c",".cc",".cpp"):
                continue
            require(not re.search(r"\bmd::parse\s*\(",f.read_text(encoding="utf-8-sig")),
                    "deprecated C++ grammar: " + str(f))
    print("PASS: Stun uses one imported SaltsUtils Markdown frontend")

if __name__ == "__main__":
    main()

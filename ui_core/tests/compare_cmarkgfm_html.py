#!/usr/bin/env python3
"""Compare actual C11 Markdown-rendered HTML to independent cmark-gfm.

This is not an AST comparison: stdin -> Stun C11 parser -> C11 RichDoc HTML
and stdin -> cmarkgfm HTML are two independent processing paths. Every gated
case requires exact byte-for-byte equality, including tags and whitespace.
Survey cases are counted separately and are never silently ignored or marked
as conformance passes. The production C11 SDK has no Python dependency.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
from importlib import metadata
import json
from pathlib import Path
import subprocess
import sys

import cmarkgfm


@dataclass(frozen=True)
class Case:
    case_id: str
    markdown: str
    profile: str = "commonmark"
    gate: bool = True
    note: str = ""


CASES: tuple[Case, ...] = (
    Case("cm.atx", "## Hello\n"),
    Case("cm.strong", "Normal **Bold** text\n"),
    Case("cm.emphasis", "A *bright* day.\n"),
    Case("cm.quote", "> quote\n"),
    Case("cm.ordered", "1. alpha\n2. beta\n"),
    Case("cm.bullet", "- one\n- two\n"),
    Case("cm.fence", "\x60\x60\x60" "c\nint v;\n" "\x60\x60\x60\n"),
    Case("cm.link", "A [B](dest)\n"),
    Case("cm.inline-code", "a " "\x60" "code" "\x60" " b\n"),
    Case("cm.rule", "---\n"),
    Case("cm.setext", "first\n===\n"),
    Case("cm.softbreak", "first\nsecond\n"),
    Case("cm.escaped", "a \\*b\n"),
    Case("cm.emoji", "中😀 👩‍💻\n"),
    Case("cm.empty", ""),
    Case("gfm.strikethrough", "a ~~red~~ day\n", "gfm"),
    Case("gfm.table", "| A | B |\n| --- | --- |\n| one | two |\n", "gfm"),
    Case("survey.cm.hardbreak", "first  \nsecond\n", gate=False,
         note="RichDoc has no hard-vs-soft-break bit yet"),
    Case("survey.cm.reference", "[reference][id]\n\n[id]: https://example.org\n",
         gate=False, note="Reference-link definition resolution is pending"),
    Case("survey.cm.entity", "Tom &amp; Jerry\n", gate=False,
         note="Entity decoding is not yet implemented in C11 parser"),
    Case("survey.cm.ordered-start", "4. alpha\n5. beta\n", gate=False,
         note="Ordered-list start index not represented in RichDoc"),
    # #186: true independent cmark-gfm GFM extension byte-exact gates.
    Case("gfm.table-align",
         "| A | B |\n| :--- | ---: |\n| x | y |\n", "gfm"),
    Case("gfm.table-escaped-uneven",
         "| A | B | C |\n| :--- | :---: | ---: |\n"
         "| a\\|b | middle | z |\n| short |\n", "gfm"),
    # Normative GFM cell boundaries are recognized BEFORE inline spans:
    # an UNescaped pipe splits cells even inside backticks. Example 200
    # independently covers the escaped form which must remain one cell.
    Case("gfm.table-code-pipe-and-uneven",
         "| A | B | C |\n| :--- | :---: | ---: |\n"
         "| a\\|b | " "\x60" "x|y" "\x60" " | z |\n| short |\n", "gfm"),
    Case("gfm.table-escaped-code-pipe-example-200",
         "| f\\|oo |\n| ------ |\n"
         "| b " "\x60" "\\|" "\x60" " az |\n"
         "| b **\\|** im |\n", "gfm"),
    Case("gfm.task", "- [x] completed\n- [ ] remaining\n", "gfm"),
    Case("gfm.task-ordered", "3. [X] done\n4. [ ] later\n", "gfm"),
    # GFM spec §4.10 Examples 198-205 and §5.3 Examples 279-280.
    # Exact-byte HTML parity is with the installed independent cmark-gfm
    # reference; official tasklist examples use a different HTML attribute
    # ordering, so normative semantic equivalence is not mislabeled as
    # official-example literal serialization. These are GFM-only gates.
    Case("gfm.official-198",
         "| foo | bar |\n| --- | --- |\n| baz | bim |\n", "gfm"),
    Case("gfm.official-199",
         "| abc | defghi |\n:-: | -----------:\nbar | baz\n", "gfm"),
    Case("gfm.official-200",
         "| f\\|oo  |\n| ------ |\n| b " "\x60" "\\|" "\x60" " az |\n"
         "| b **\\|** im |\n", "gfm"),
    Case("gfm.official-201",
         "| abc | def |\n| --- | --- |\n| bar | baz |\n> bar\n", "gfm"),
    Case("gfm.official-202",
         "| abc | def |\n| --- | --- |\n| bar | baz |\nbar\n\nbar\n", "gfm"),
    Case("gfm.official-203",
         "| abc | def |\n| --- |\n| bar |\n", "gfm"),
    Case("gfm.official-204",
         "| abc | def |\n| --- | --- |\n| bar |\n"
         "| bar | baz | boo |\n", "gfm"),
    Case("gfm.official-205",
         "| abc | def |\n| --- | --- |\n", "gfm"),
    Case("gfm.official-279",
         "- [ ] foo\n- [x] bar\n", "gfm"),
    Case("gfm.official-280",
         "- [x] foo\n  - [ ] bar\n  - [x] baz\n- [ ] bim\n", "gfm"),
    # #186 second GFM admission/security slice: parity of backslash runs,
    # nested vs loose task ownership, invalid markers, and inline content.
    # These are extension-specific cmark-gfm byte gates, not CommonMark
    # official 652 identities or published official GFM numbered examples.
    Case("gfm.table-odd-backslash-run",
         "| Name | Value |\n| --- | --- |\n"
         + r"| a\\\|b | tail |" + "\n", "gfm"),
    Case("gfm.table-even-backslash-run",
         "| Name | Value |\n| --- | --- |\n"
         + r"| a\\|b | tail |" + "\n", "gfm"),
    Case("gfm.task-empty-marker", "- [ ]\n- [x]\n", "gfm"),
    Case("gfm.task-loose-list",
         "- [x] shipped\n\n- [ ] pending\n", "gfm"),
    Case("gfm.task-nested-ordered",
         "- [x] parent\n  1. [ ] nested\n  2. [X] done\n- [ ] tail\n", "gfm"),
    Case("gfm.task-inline-rich-text",
         "- [x] **bold** " "\x60" "code" "\x60" " and [link](/go)\n", "gfm"),
    Case("gfm.task-invalid-markers",
         "- [x]done\n- [y] not task\n- \\[x] escaped\n", "gfm"),
    # Quoted GFM TABLEs: exact explicit quote-depth matching only. The same
    # cmark-gfm HTML byte oracle guards root and nested GFM table rendering.
    Case("gfm.quote-table-align",
         "> | H | N |\n> | :--- | ---: |\n"
         "> | a\\|b | **bold** |\n", "gfm"),
    Case("gfm.nested-quote-table",
         "> > | A | B |\n> > | --- | --- |\n> > | x |\n", "gfm"),
    Case("gfm.quote-table-then-paragraph",
         "> | Key | Value |\n> | --- | --- |\n"
         "> | a | b |\n>\n> After\n", "gfm"),
    # #186: explicit QUOTE-owned table admission; verify complete
    # container ownership / source-boundary transitions independently
    # against cmark-gfm byte-for-byte, without a second parser.
    Case("gfm.quote-table-crlf",
         "> | L | R |\r\n> | :--- | ---: |\r\n"
         "> | a\\|b | z |\r\n", "gfm"),
    Case("gfm.quote-table-no-final-lf",
         "> | L | R |\n> | --- | --- |\n> | x | y |", "gfm"),
    Case("gfm.quote-table-root-dedent",
         "> | L | R |\n> | --- | --- |\n> | x | y |\n"
         "Outside\n", "gfm"),
    Case("gfm.quote-table-then-nested-quote",
         "> | L | R |\n> | --- | --- |\n> | x | y |\n"
         "> > nested\n", "gfm"),
    Case("gfm.quote-two-tables-separated",
         "> | A | B |\n> | --- | --- |\n> | a | b |\n>\n"
         "> | C | D |\n> | --- | --- |\n> | c | d |\n", "gfm"),
    Case("gfm.quote-table-after-fence",
         "> ```\n> | A | B |\n> | --- | --- |\n"
         "> ```\n> | C | D |\n> | --- | --- |\n> | c | d |\n", "gfm"),
    Case("gfm.quote-table-unmarked-delimiter",
         "> | A | B |\n| --- | --- |\n", "gfm"),
    Case("gfm.quote-table-unmarked-row",
         "> | A | B |\n> | --- | --- |\n> | a | b |\n"
         "| c | d |\n", "gfm"),
    # #186: separated LIST_ITEM table bodies must preserve exact content
    # indentation and explicit QUOTE ownership, without reparsing in HTML.
    Case("gfm.list-table-separated",
         "- intro\n\n  | H | V |\n  | :--- | ---: |\n"
         "  | a\\|b | **yes** |\n", "gfm"),
    Case("gfm.quote-list-table",
         "> - intro\n>\n>   | H | V |\n>   | --- | --- |\n"
         ">   | x |\n", "gfm"),
    Case("gfm.nested-list-table",
         "- outer\n  - inner\n\n"
         "    | H | V |\n    | --- | --- |\n"
         "    | x | y |\n", "gfm"),
    Case("survey.cm.image", "![alt](image.png)\n", gate=False,
         note="C11 renderer rejects unsupported image semantics"),
    Case("survey.cm.raw-html", "<script>unsafe()</script>\n", gate=False,
         note="Raw HTML not supported; unsafe output must not be passed through"),
)


def reference_html(case: Case) -> bytes:
    fn = (cmarkgfm.markdown_to_html if case.profile == "commonmark"
          else cmarkgfm.github_flavored_markdown_to_html)
    return fn(case.markdown).encode("utf-8")


def native_html(executable: Path, case: Case) -> bytes:
    process = subprocess.run(
        [str(executable)], input=case.markdown.encode("utf-8"),
        capture_output=True, check=False, timeout=15,
    )
    if process.returncode:
        error = process.stderr.decode("utf-8", "replace").strip()
        raise ValueError(f"native status {process.returncode}: {error[:1600]}")
    if process.stderr:
        raise ValueError("native executable printed unexpected stderr: "
                         + process.stderr.decode("utf-8", "replace")[:400])
    return process.stdout


def describe_difference(got: bytes, expected: bytes) -> str | None:
    if got == expected:
        return None
    size = min(len(got), len(expected))
    pos = next((i for i in range(size) if got[i] != expected[i]), size)
    left = got[max(0, pos - 32):pos + 60].decode("utf-8", "backslashreplace")
    right = expected[max(0, pos - 32):pos + 60].decode("utf-8", "backslashreplace")
    return f"byte {pos}, lengths {len(got)} != {len(expected)}; got={left!r} expected={right!r}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renderer", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if not args.renderer.is_file():
        print(f"INFRASTRUCTURE ERROR: C11 HTML executable absent: "
              f"{args.renderer}", file=sys.stderr)
        return 2
    if not CASES:
        print("INFRASTRUCTURE ERROR: no test fixtures", file=sys.stderr)
        return 2
    official_ids = tuple(
        c.case_id.split("-", 1)[1] for c in CASES
        if c.case_id.startswith("gfm.official-") and c.gate
    )
    expected_ids = tuple(map(str, list(range(198, 206)) + [279, 280]))
    if official_ids != expected_ids:
        print("INFRASTRUCTURE ERROR: GFM official case identity drift:"
              f" {official_ids!r} != {expected_ids!r}", file=sys.stderr)
        return 2

    results: list[dict] = []
    gated_total = gated_passed = survey_total = survey_matched = 0
    gfm_official_total = gfm_official_passed = 0
    for case in CASES:
        record: dict = {
            "id": case.case_id, "profile": case.profile,
            "gate": case.gate, "note": case.note,
            "markdown": case.markdown,
        }
        try:
            expected = reference_html(case)
            record["reference_html"] = expected.decode("utf-8")
            record["reference_sha256"] = hashlib.sha256(expected).hexdigest()
            actual = native_html(args.renderer, case)
            record["actual_html"] = actual.decode("utf-8", "strict")
            record["actual_sha256"] = hashlib.sha256(actual).hexdigest()
            difference = describe_difference(actual, expected)
            record["matched"] = difference is None
            record["difference"] = difference
        except (RuntimeError, UnicodeError, ValueError, OSError,
                subprocess.TimeoutExpired) as exc:
            record["matched"] = False
            record["difference"] = str(exc)

        if case.gate:
            gated_total += 1
            gated_passed += int(record["matched"])
            if case.case_id.startswith("gfm.official-"):
                gfm_official_total += 1
                gfm_official_passed += int(record["matched"])
        else:
            survey_total += 1
            survey_matched += int(record["matched"])
        status = "PASS" if record["matched"] else "DIFF"
        kind = "gate" if case.gate else "survey"
        print(f"[{kind}][{status}] {case.case_id}"
              + (": " + record["difference"] if record["difference"] else ""))
        results.append(record)

    report = {
        "schema_version": 1,
        "test": "C11 RichDoc HTML -> independent cmarkgfm exact-byte comparison",
        "cmarkgfm_version": metadata.version("cmarkgfm"),
        "gate": {"passed": gated_passed, "total": gated_total},
        "gfm_official_subset": {"passed": gfm_official_passed,
                                "total": gfm_official_total,
                                "ids": list(expected_ids),
                                "reference": "cmarkgfm HTML byte equality",
                                "official_html_note": "HTML attribute serialization may differ from the published GFM prose examples"},
        "survey": {"matched": survey_matched, "total": survey_total},
        "cases": results,
        "note": "Strict GFM/CommonMark HTML subset comparison; not complete normative"
                " spec coverage. Survey results do not count as gated passes.",
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"RESULT: cmarkgfm HTML {gated_passed}/{gated_total} gated matches,"
          f" {survey_matched}/{survey_total} diagnostic matches; "
          f"report={args.report}")
    return 0 if gated_passed == gated_total else 1


if __name__ == "__main__":
    sys.exit(main())

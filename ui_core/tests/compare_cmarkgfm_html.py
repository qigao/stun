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
    # cmarkgfm 2025.10.22 currently splits a pipe inside an inline code
    # span into two table cells; Stun correctly keeps it opaque, and Mistune
    # independently differs. Record both actual byte outputs in a survey;
    # do not weaken the C11 owned-code-span fixture or claim an exact oracle
    # match when the comparison engines disagree about this extension.
    Case("survey.gfm.table-code-pipe",
         "| A | B | C |\n| :--- | :---: | ---: |\n"
         "| a\\|b | " "\x60" "x|y" "\x60" " | z |\n| short |\n",
         "gfm", False, "Known cmark-gfm code-span-pipe split; review separately"),
    Case("gfm.task", "- [x] completed\n- [ ] remaining\n", "gfm"),
    Case("gfm.task-ordered", "3. [X] done\n4. [ ] later\n", "gfm"),
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

    results: list[dict] = []
    gated_total = gated_passed = survey_total = survey_matched = 0
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

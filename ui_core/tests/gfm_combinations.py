#!/usr/bin/env python3
"""Deterministic #186 GFM composition fixtures for the *test-only* HTML oracle.

All cases use public Markdown input. This file is not a parser, transformer,
reference implementation, or production dependency. The existing C11
renderer and independently installed cmark-gfm remain the two execution paths.

Cartesian product: 10 owner frames x 4 table shapes x 2 EOL styles
x 2 EOF forms = 160 strict reference cases. Deliberately exclude the two
separately pinned GFM §5.3 task/cmark-gfm semantic disagreements.
"""
from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json


@dataclass(frozen=True)
class GfmCombination:
    case_id: str
    markdown: str
    owner: str
    table: str
    eol: str
    tail: str


_TICK = chr(96)
_TABLE_SHAPES: tuple[tuple[str, tuple[str, ...]], ...] = (
    ("plain", (
        "| A | B |",
        "| --- | --- |",
        "| alpha | beta |",
        "| next | last |",
    )),
    ("alignment", (
        "| Name | Value |",
        "| :--- | ---: |",
        "| a | 1 |",
        "| b | 2 |",
    )),
    ("escaped-inline", (
        "| L | R |",
        "| :---: | ---: |",
        r"| a\|b | **bold** |",
        f"| {_TICK}x\\|y{_TICK} | [link](/go) |",
    )),
    ("ragged", (
        "| A | B | C |",
        "| --- | :---: | ---: |",
        "| short |",
        "| one | two | three | surplus |",
    )),
)

# (id, source prelude, first-table-line prefix, next-table-line prefix)
# A list-owned table's first line is the list-item paragraph; its delimiter
# and body rows must instead be indented to the actual list content column.
_OWNER_FRAMES: tuple[tuple[str, str, str, str], ...] = (
    ("root", "", "", ""),
    ("quote1", "", "> ", "> "),
    ("quote2", "", "> > ", "> > "),
    ("list-bullet", "", "- ", "  "),
    ("list-ordered7", "", "7. ", "   "),
    ("quote-list", "", "> - ", ">   "),
    ("nested-list", "- outer\n", "  - ", "    "),
    ("quote-nested-list", "> - outer\n", ">   - ", ">     "),
    ("task-checked", "- [x] parent\n\n", "  ", "  "),
    ("task-open", "- [ ] parent\n\n", "  ", "  "),
)

EXPECTED_COUNT = 10 * 4 * 2 * 2


def generated_cases() -> tuple[GfmCombination, ...]:
    """Return immutable, uniquely named bounded cases, never random samples."""
    output: list[GfmCombination] = []
    for owner, prelude, first_prefix, row_prefix in _OWNER_FRAMES:
        for table, lines in _TABLE_SHAPES:
            source = prelude + first_prefix + lines[0] + "\n"
            source += "".join(row_prefix + line + "\n" for line in lines[1:])
            for eol, delimiter in (("lf", "\n"), ("crlf", "\r\n")):
                converted = source.replace("\n", delimiter)
                for tail in ("nl", "eof"):
                    markdown = (converted if tail == "nl"
                                else converted[:-len(delimiter)])
                    assert markdown and (tail == "nl") == markdown.endswith(delimiter)
                    output.append(GfmCombination(
                        case_id=f"gfm.mx.{owner}.{table}.{eol}.{tail}",
                        markdown=markdown,
                        owner=owner,
                        table=table,
                        eol=eol,
                        tail=tail,
                    ))
    ids = [case.case_id for case in output]
    if len(output) != EXPECTED_COUNT or len(ids) != len(set(ids)):
        raise AssertionError("GFM matrix cardinality/identity drift")
    return tuple(output)


def manifest_sha256(cases: tuple[GfmCombination, ...]) -> str:
    """Fingerprint *all* generated fixture source bytes and stable identities."""
    payload = [(c.case_id, c.markdown) for c in cases]
    raw = json.dumps(payload, ensure_ascii=False, separators=(",", ":"))
    return hashlib.sha256(raw.encode("utf-8")).hexdigest()


if __name__ == "__main__":
    result = generated_cases()
    print(f"GFM #186 deterministic matrix {len(result)}/{EXPECTED_COUNT}"
          f" sha256={manifest_sha256(result)}")

# CommonMark remaining cases: classified remediation

## Evidence and gate

Reference Stun HEAD: `29cae2701314b72c8c40b8139f7349b240ad6364` (PR #223).
Runtime: `38424026066`; artifact: `11828112882`.
CommonMark 0.31.2, `tools/md/test/spec.txt`, 652 cases.
Canonical `(ID, Markdown, expected HTML)` SHA-256:
`c784c98aa6f8a1db1292dfd883f3b6743f2ad66578f89fae1fc9223f208f7bfd`.

Recomputed actual output: **549 exact + 93 different + 10 unsupported = 652**.
All **103 unpassed** cases have one primary remediation category. Classification
is NOT a pass exemption. This change implements diagnostic accounting, not the
remaining parser/renderer features. Keep PR #223 Draft / DO NOT MERGE.

| Category | Different | Unsupported | Total | Owner |
|---|---:|---:|---:|---|
| Raw HTML grammar / render policy | 72 | 0 | 72 | #184 |
| Typed images / references / title / alt | 12 | 10 | 22 | #224; parent #183 |
| URI scheme policy separation | 4 | 0 | 4 | #183 + #184 |
| Unicode provider capabilities | 3 | 0 | 3 | Salts #1164 / #1192 |
| Quote/list reference definitions | 2 | 0 | 2 | #225; parents #181/#183 |
| Total | 93 | 10 | 103 | |

## Images: fix representation before rendering (#224)

IDs: 517, 520, 531, 572-589, 591. These are 22 image-related failures,
including three outside the Images section. That section has 19 failures / 22
examples; existing matches 590, 592, 593 are literal/invalid-syntax controls,
not evidence that real image rendering works.

Twelve reference forms currently produce `!` plus LINK:
573, 576, 577, 582, 583, 584, 585, 586, 587, 588, 589, 591.
Ten inline/nested IMAGE cases are unsupported:
517, 520, 531, 572, 574, 575, 578, 579, 580, 581.

Source: reference binding is under `if(!image)`; decoded inline image title is
stored only for LINK; the HTML writer has no IMAGE case. Adding `<img>` output
alone cannot recover lost title data or repair reference-image recognition.

Implementation order:
1. Share admission/opacity for inline and full/collapsed/shortcut reference
   images; create IMAGE, not `!` + LINK.
2. Preserve owned src, optional/empty title and inline descendants with explicit
   validator rules; do not place LINK_TITLE under an invalid parent.
3. Flatten alt by bounded traversal of the same owned subtree: text, emphasis,
   code, links and nested images; never serialize markup or href/title as alt.
4. Review safe src policy separately from mailto-capable href. Retain scheme,
   control-byte and attribute-injection rejection. Serialization never fetches
   network resources. Preserve node/output failure atomicity and cleanup.

Gate: all 22 exact outputs, IMAGE-kind and title assertions, empty/nested alt,
malformed reference negatives, no LINK-under-LINK regression, source mutation/
free and document isolation, exact bounds and installed-SDK sanitizers.

## Container references: use real block ownership (#225)

IDs: 218 (quote definition resolves earlier document text), 317 (definition
inside loose list item). Root-only collection is an implementation restriction,
not a CommonMark prohibition. Use real/shared canonical block traversal, source
order, one bounded document-owned map and first-definition-wins. Remove only
validated definition text; retain empty quotes and correct list tightness.

Do not strip quote markers or indentation in a second guessed grammar. Reconcile
old root-only negative fixtures against normative behavior; preserve real code,
fence and paragraph-continuation negatives. Gate mixed root/quote/list duplicates,
forward references, capacity rollback and raw-source lifetime.

## Raw HTML: missing grammar AND policy separation (#184)

This is not a 72-line escape fix:
- 44 block cases: 148-191. Seven block types, interruption, blank/termination
  rules, code opacity, containers and exact source ownership.
- 13 inline cases: 613-617, 623, 625-631. Tags, comments, processing instructions,
  declarations, CDATA and proper inline token spans.
- 15 cross-section cases: 21, 31, 201, 308, 309, 344, 475-477, 491, 494,
  524, 536, 642, 643. HTML also owns escape/emphasis/link/list/break boundaries.

Use typed RAW_HTML_BLOCK / RAW_HTML_INLINE owned spans and one AST traversal with
explicit versioned policies: SafeApplicationHtml remains the default;
SpecConformanceHtml is an explicitly selected test-only serializer. Never
unescape rendered strings or relax production checks just to raise the score.
642/643 demonstrate an actual parser boundary defect: an HTML attribute newline
must not become Markdown `<br />`.

Gate: separate normative exact and production-safe tests on the same AST;
malformed markup, malicious attributes/URLs, source spans, cleanup/capacity,
all 72 IDs, and no raw script execution.

## URI schemes: preserve the production allowlist (#183/#184)

IDs: 596 (`irc`), 598 (`a+b+c`), 599 (`made-up-scheme`), 601 (`localhost`).
CommonMark syntax is broader than the production HTTP/HTTPS/mailto policy.
Separate syntactic token admission from rendering policy on the same AST.
Explicit conformance output may represent the official schemes; the safe
entrypoint retains rejection/escaped text and hostile-scheme/control tests.
Calling a difference a policy issue does not make its HTML byte-exact.

## Unicode: two distinct upstream capabilities

206 / 540 require full default Unicode case folding of lookup keys via
[Salts #1192](https://github.com/qigao/salts/issues/1192); Greek case matching and
capital sharp-S expansion are not locale lowercase or NFC. Preserve raw labels.
354 instead requires Unicode General_Category P*/S* for emphasis flanking,
[Salts #1164](https://github.com/qigao/salts/issues/1164): currency symbols count
as punctuation here. Consume the canonical published C11 provider. Test
expansions, invalid input, output budgets, reentrancy and exact SDK provenance;
no private Unicode tables, locale substitutes or compatibility fallbacks.

## Executable accounting and order

`tests/commonmark_failure_classes.json` is the reviewed 103-ID ledger.
`tests/classify_commonmark_failures.py` checks the complete corpus, recomputes
actual equality, validates totals, requires disjoint IDs and rejects unclassified
regressions. It never modifies expected HTML or the permanent passing-ID list.
The existing `gate_commonmark_nonregression.py` embeds this partition in its
existing JSON artifact; no extra workflow or SDK-install stage. Genuine resolved
IDs remain visible in history; promotion still requires independent exact-HEAD
CI evidence. This diagnostic commit makes no production parser changes.

```sh
python -m unittest discover -s ui_core/tests -p 'test_classify_commonmark_failures.py'
python ui_core/tests/classify_commonmark_failures.py \
  --survey path/to/markdown-commonmark-652-survey.json --out classification.json
```

Work order: IMAGE recognition/title/alt and container references; typed raw-HTML
grammar plus explicit policy; URI integration on that policy contract. The two
Salts Unicode tasks are independent upstream tracks. Keep the existing 549 exact
IDs and production security gating throughout.

CommonMark examples: John MacFarlane and contributors, CommonMark 0.31.2,
[CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/),
[specification](https://spec.commonmark.org/0.31.2/).

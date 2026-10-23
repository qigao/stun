# CommonMark conformance and remaining integration

## Current qualified contract

The single C11 frontend and owned RichDoc renderer implement all **652/652**
official CommonMark 0.31.2 examples under explicitly selected
`SpecConformanceHtml`. Production `SafeApplicationHtml` remains a separate
contract: **576/576** approved official IDs must stay byte-exact; the remaining
**76** outputs intentionally differ because raw HTML and URI schemes have a
stricter production policy. No example is unsupported in either survey.

These counts are executable gates, not parse-success or source-marker checks:

- `tests/commonmark_0_31_2_spec_baseline.json`: all 652 normative IDs.
- `tests/commonmark_0_31_2_baseline.json`: the independent 576 SAFE IDs.
- `tests/gate_commonmark_spec_nonregression.py` and
  `tests/gate_commonmark_nonregression.py`: recompute exact HTML equality and
  validate the complete official corpus digest.
- `tests/commonmark_failure_classes.json`: historical 103-ID investigation
  ledger, also used to classify the remaining SAFE policy differences. Its
  resolved image, Unicode and container-reference entries are not current
  normative parser failures.

Canonical `(ID, Markdown, expected HTML)` SHA-256:
`c784c98aa6f8a1db1292dfd883f3b6743f2ad66578f89fae1fc9223f208f7bfd`.
Do not change official expected HTML or relax either ID baseline to raise a
score. SPEC is an explicit test serializer; untrusted applications retain the
SAFE entrypoint. See [render policies](RAW_HTML_RENDER_POLICIES.md) and the
[HTML output decision](ADR_MARKDOWN_HTML_OUTPUT.md).

## Verified evidence

[CI 39017661257](https://github.com/qigao/stun/actions/runs/39017661257/job/117352994455)
at exact source `65e9b55bbd083690c42f554b68f82c73268e68e8` passed:

- all 20 standalone native C11 tests against installed Salts;
- SPEC 652/652 and SAFE 576/576 official HTML;
- independent cmark-gfm 466/466 plus normative GFM task 2/2, combined 468/468;
- the versioned 360/360 GFM composition matrix;
- actual ASan/UBSan with `detect_leaks=1`.

The 2026-10-23 issue review repeated the existing CMake/CTest and both complete
surveys on master `6f5ed488d8ad1d767a7eacca8cd9491412e89e0a`, using the official
Salts 2.3.0-rc.17 installed SDK. The same 20/20, 652/652, 576/576 and 468/468
results passed under ASan/UBSan. Local LSan cannot inspect `/proc/task`; the
remote strict result above remains the leak-check evidence. The independent
local reference was cmarkgfm 2025.10.22. Reference-parser output is recorded
separately from the official expected HTML.

## Completed feature issues

- #182: delimiter-run emphasis/strong and exact-length code spans.
- #183: links, references, title/alt, images and Unicode lookup semantics.
- #184: typed raw HTML and separate SPEC/SAFE rendering policies.
- #185: hard/soft breaks, escapes and entity decoding.
- #186: table alignment and task-list HTML, including the qualified nested
  composition and metadata-safety fixtures.

Those issues no longer represent the earlier 549-ID or 649-ID parser gaps.
Passing the official CommonMark corpus does not claim every possible GFM
extension, malicious input, platform or consumer has been qualified.

## Remaining work belongs to its current owner

- #181: additional adversarial nested-container/resource-failure coverage and
  any justified simplification of the single frontend's block admission.
- #172: remaining consumer integration and retirement of duplicate production
  grammar paths. MarkdownWidget already consumes the canonical C11 RichDoc;
  Tango's production cutover is still separate.
- #167: shared geometry/layout, reflected state/lifetimes and installed SDK
  delivery beyond the completed Markdown subset.
- #165: terminal runtime integration and cell/width/lifecycle contracts.
- #31: actual application distribution and remaining supported platform gates.

Keep capacity/ownership rollback, malformed RichDoc rejection, source lifetime,
URL safety and independent consumer validation in the native tests. New features
must preserve all existing strict IDs and distinguish normative HTML from the
production safety policy.

## Historical investigation

The earlier `29cae2701314b72c8c40b8139f7349b240ad6364` / PR #223 checkpoint was
**549 exact + 93 different + 10 unsupported**. Its 103 unpassed examples were
classified into raw HTML (72), images (22), URI policy (4), Unicode (3) and
container references (2). That work drove the source fixes and policy split
now qualified above. It is historical evidence, not a current instruction to
keep already-integrated branches Draft or to reimplement those features.

CommonMark examples: John MacFarlane and contributors, CommonMark 0.31.2,
[CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/),
[specification](https://spec.commonmark.org/0.31.2/).

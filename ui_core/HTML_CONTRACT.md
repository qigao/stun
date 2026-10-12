# C11 RichDoc → HTML (bounded candidate)

Relates to [#172](https://github.com/qigao/stun/issues/172) and [#167](https://github.com/qigao/stun/issues/167).

This C11 renderer consumes **only** a validated `stun_ui_rich_document_view` from the single Stun C11 Markdown parser. It does not tokenize Markdown or create a second grammar; it has no GUI, Tango, Python or cmark-gfm production dependencies.

## Contract

`stun_md_render_html(&view, max_nodes, buffer, capacity, &required)` uses two passes: first measure and validate, then write exactly `required` bytes plus NUL. Use `buffer=NULL,capacity=0` to query length. On any error or insufficient capacity, no caller output bytes are written. The view and all borrowed metadata must remain live during both passes under one owner thread.

The first production-quality subset covers paragraphs, headings, emphasis/strong/del, code spans/fences, blockquotes, lists, safe links, soft breaks, rules and plain GFM tables. The source's UTF-8 bytes (including Emoji) remain unchanged except normal HTML text/attribute escaping.

Unsupported semantic types intentionally produce `STUN_UI_INVALID_ARGUMENT`: images, task checkboxes, diagrams, math, entity-specific transforms, footnotes, definition lists and unmodeled hard line breaks do **not** silently fall back to raw HTML. Unsafe URL schemes are rejected rather than emitted into `href`.

The IR currently lacks loose-vs-tight list facts, hard-vs-soft line break distinctions, GFM table alignment, and ordered-list starting index; therefore **this renderer does not claim full CommonMark/GFM conformance**. These fields must be supplied by the canonical parser/typed RichDoc, not guessed by the renderer.

## Verification

`stun_markdown_html_test` is a real strict C11 executable using the installed `Stun::UICore`, checked by the existing installed-Salts SDK ASan/UBSan workflow. It asserts 17 exact HTML golden outputs, atomic capacity rejection, safe link admission, and unsupported image handling.

The next gate must **also** compare actual C11-produced HTML against independent `cmarkgfm` HTML on a complete, versioned sample suite. Mistune AST parity alone is not an HTML conformance proof, and our test-only HTML examples do not replace that independent oracle.

## Independent HTML oracle (test-only)

The test program `stun_markdown_html_dump` runs the **real** C11 Markdown
parser and C11 RichDoc HTML renderer. It accepts at most 1 MiB of Markdown
from stdin and writes only HTML bytes to stdout; failures use nonzero exit
and stderr, never fallback to C++ or a Python parser.

`ui_core/tests/compare_cmarkgfm_html.py` independently processes the *same*
input through Python `cmarkgfm`: CommonMark examples use `markdown_to_html`,
GFM examples use `github_flavored_markdown_to_html`. It asserts **exact HTML
bytes**, including whitespace, escaping and structural tags, for an
explicit 17-case support subset. The separate diagnostic survey records
unsupported/reference-link/table-alignment/task/HTML-entity differences
without counting them as passes or bypassing failure in a supported case.

The existing Unicode runtime installed-Salts SDK gate compiles the C11
executable with its ASan/UBSan settings and runs the Python oracle inside
a separate ephemeral virtualenv. The `cmarkgfm` package is not linked into
Stun or required by any consumer; its resolved version and per-fixture
first-byte differences are saved in `markdown-cmarkgfm-html.json`.

**Important:** 17 supported fixtures passing means only *subset* byte-for-byte
HTML equality, not normative full CommonMark/GFM compliance. Full official
spec coverage, reference links, list tightness, ordered starts, alignment
metadata, hardbreaks and task checkbox HTML remain required by #172.

## Full CommonMark 0.31.2 survey (652 official examples)

The repo's existing `tools/md/test/spec.txt` contains **652 official
CommonMark 0.31.2 examples**, with canonical expected HTML. The test-only
`ui_core/tests/survey_commonmark_spec.py` extracts every fenced example
(substituting the spec's right-arrow TAB notation), invokes the **actual**
C11 HTML executable and independently computes cmarkgfm reference HTML.

It emits `markdown-commonmark-652-survey.json` with every numbered fixture's
raw input, official expected HTML, cmarkgfm HTML, actual C11 HTML or explicit
unsupported status, and first byte difference. It reports C11-vs-official,
C11-vs-cmarkgfm and cmarkgfm-vs-official totals separately. Missing fixtures,
unexpected interpreter/tool failures, native crashes, sanitizer reports or
unexpected partial writes fail the survey immediately. The 652 semantic
matches are **diagnostic only**, because the current C11 renderer deliberately
rejects many unsupported grammar features. Full spec compliance is a separate
future gate, not inferred from the 17-fixture supported-subset HTML comparison.

## Indented code subset (#177)

The C11 parser now distinguishes \`STUN_UI_BLOCK_CODE_INDENTED\` from fenced
code. Its payload is owned literal UTF-8 text with four *column* indentation
removed using CommonMark tab stops; CRLF and EOF are emitted as normalized LF.
Interior blank lines remain pending until a following indented content line
proves they belong to the code block; leading/trailing blank lines are dropped.
Raw Markdown is untouched. The renderer escapes the text as \`<pre><code>\`
without an info-string or a second Markdown grammar.

\`stun_markdown_indented_test\` checks native tree ancestry, exact rendered
HTML, bounded output, and rejection without partial writes. The independent
official-spec test \`gate_commonmark_indented.py\` checks explicit numbered
CommonMark 0.31.2 samples against **official expected HTML**. All 652 examples
remain in the separately counted diagnostic survey. Loose list precedence
(#108/#109) remains diagnostic until typed loose-list semantics are added.

## List-container conformance subset (#181)

A bounded C11 list-container frame stack now owns list/item continuation,
marker-family boundaries, nested lists, blank lines and selected indented
code/quote items. Owned binary `stun_ui_list_semantics` metadata records the
exact ordered start (including zero), list marker and loose/tight distinction.
The existing HTML renderer consumes these typed facts without reparsing
Markdown. Legacy projected lists with zero metadata retain tight semantics.

`stun_markdown_containers_test` now has **13** exact HTML/AST fixtures plus
an atomic depth-exhaustion regression. The explicit official CommonMark
0.31.2 gate covers #108/#109 and list precedence #254/#256/#301/#302/#304/#305
alongside the previous 13 cases: **21 strict IDs** (not full 652 compliance).
The full **652**-example diagnostic remains separate. Other lazy/container,
task and raw-HTML semantics stay under #181/#172 and must not be reported as
supported without passing the corresponding official HTML.

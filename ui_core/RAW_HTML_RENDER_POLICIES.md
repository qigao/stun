# ADR: Typed Raw HTML and Explicit Render Policies

Status: incremental C11 contract for Stun #184; PR #223 remains Draft / DO NOT MERGE.

## Decision

Use one existing Markdown grammar and one document-owned RichDoc. Append
RAW_HTML_INLINE and RAW_HTML_BLOCK as stable enum values without renumbering
previous kinds. Every admitted raw token is a leaf with an owned text-arena
byte range. The renderer neither parses raw source nor keeps runtime state.

Introduce an explicit render policy on the same bounded two-pass AST traversal.
The existing stun_md_render_html entrypoint continues to select the SAFE
APPLICATION policy without changing its signature. The opt-in SPEC CONFORMANCE
policy emits typed raw HTML literally, and is for trusted normative testing
only: NEVER feed its output from untrusted input into a WebView or browser.
The SAFE policy escapes all raw token bytes, including scripts/attributes.
No automatic policy fallback, unsafe URL allowlist widening, or DOM execution.

## First slice

Only syntactically complete single-physical-line HTML start/end tags,
comments, processing instructions, declarations and CDATA are recognized
in the one inline parser. Shared quoted-attribute delimiter
opacity and the existing code-span/autolink priority are preserved. Invalid
or incomplete tags remain ordinary safely escaped TEXT. Admitted tokens
copy their exact source bytes into RichDoc before returning to the caller.

Independent installed C11 fixtures compare the exact official HTML output
for #613, #614, #617, #623, #627, #628, #629, #630 and #631 under
the explicit SPEC policy.
Those same cases must keep previous escaped HTML under the default SAFE
policy. Also reject invalid policy values, unchanged output after capacity
failure, malformed tags, hostile script tokens and one-node-short budgets.

## Remaining work in the Raw HTML category

- Implement seven HTML block types #148-#191 in the SAME existing canonical
  block parser, with paragraph interruption and blank-line termination.
- Support complete inline token grammar including multiline quoted tags
  #615/#616/#642/#643, multiline comments and other HTML token spans,
  plus interactions with emphasis, links, lists and code.
- Emit RAW_HTML_BLOCK only from recognized block ownership; reject fake
  HTML declarations that are really escaped ordinary text.
- Add separate full 652-case SPEC profile diagnostic/report and immutable
  ID gate. The SPEC report is non-gating until independently qualified. The existing full 652-case SAFE survey, 573-ID strict baseline
  and fixture digest stay unchanged. An explicit-policy exact match is
  not evidence that the production SAFE renderer should pass raw markup.
- Require complete C11/installed SDK/sanitizer/FlexUI/Mesa validation,
  source ownership after input mutation, bounded node/bytes, and zero output
  on all failures. No consumer cutover before independent release gates.

## Out of scope

No alternative markdown frontend, runtime persistence, silent fallback,
unbounded HTML DOM, external network fetch, automatic retry, or alteration
of the public SafeApplicationHtml default.

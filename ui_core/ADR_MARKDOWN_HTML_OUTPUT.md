# ADR: Markdown whitespace ownership and HTML output profiles

**Status:** Decided for the existing SafeApplicationHtml path; the proposed
SpecConformanceHtml profile and typed RAW_HTML nodes are **not yet implemented**.
**Scope:** Stun C11 Markdown / RichDoc (#181, #184, #172).
**Evidence:** CommonMark 0.31.2, exact HEAD `df240a0`,
[Runtime #38344397484](https://github.com/qigao/stun/actions/runs/38344397484)
(14/14 installed C11 CTests; 242/242 HTML fixtures; 509/652 official exact
matches, 8 unsupported, 135 differences; sanitizers and Mesa green).

## 1. Parsing and whitespace are semantic decisions, not a global trim

Keep the original Markdown bytes in the document's immutable raw-source owner.
Build separately owned semantic text spans in the typed RichDoc. Context matters:

- Root block indentation participates in block admission, not in a global
  string-level leading/trailing whitespace deletion.
- A root fenced-code opener consumes zero to three ASCII indent spaces;
  a later code line loses **at most that many** leading ASCII spaces.
  Extra leading spaces, all trailing spaces and line-ending bytes remain
  code payload. The parser's current root-fence implementation and exact
  official CommonMark #131–133 tests enforce this rule.
- Indented code removes its own four visual columns. List and blockquote code
  must first match the container's content column; tabs advance to the next
  four-column stop. If a consumed tab overshoots a container boundary, its
  remaining visual columns must not disappear. Official #5–7 still fail and
  remain tracked by #181: do **not** paper over them using `trim()`.
- Code spans apply CommonMark's own newline and outer-space normalization;
  ordinary text and inline tokens retain independently owned semantics.
- HTML serialization never modifies Markdown source or reparses it.

## 2. Serialization contexts are different types of output

The existing production-safe `stun_md_render_html` traverses a validated
typed RichDoc in two passes: measure/validate, then write all bytes or none.
The native C11 HTML5 renderer already separates these responsibilities:

| Context | Required behavior | Existing implementation |
| --- | --- | --- |
| TEXT and literal CODE content | Escape `&`, `<`, `>` and `"` per the current CommonMark HTML5 byte contract; code does not interpret Markdown/HTML entities | `html_escaped(..., false)` |
| Double-quoted ATTRIBUTE (e.g. title) | Accept decoded semantic metadata, validate controls, HTML5-escape exactly once | `html_safe_title`, `html_escaped(..., true)` |
| URL-valued ATTRIBUTE (href) | First reject unsafe decoded schemes/C0 controls, then percent-normalize UTF-8/delimiters, then HTML-escape the resulting attribute | `html_safe_href`, `html_link_href` |
| Parsed HTML entity in text/title | Decode only at Markdown grammar admission, then escape semantic scalar bytes once when emitting HTML | C11 semantic entity decoding + the context writer |
| RAW_HTML block/inline | Untrusted raw markup must **not** be silently emitted by the safe renderer | Explicit typed nodes / separate policy still pending #184 |

Examples include literal code `&copy;` emitted as `&amp;copy;`, a decoded
title `A & B` emitted as `title="A &amp; B"`, and a URL payload
`/a&b` emitted as `href="/a&amp;b"`. The new exact HTML/native tests
ensure no double decoding, invented markup, partial writes, or blanket trim.

## 3. Keep HTML5 distinct from XML/XHTML

CommonMark normative comparisons target **HTML5** output, not generic XML.
An XML/XHTML format would need an explicitly selected separate serialization
profile: XML character-validity rules, attribute quoting, entity rules, and
empty-element form differ. Do not change the current HTML5 renderer into a
hybrid "HTML/XML" writer or assume the same raw-markup handling fits both.

## 4. One typed IR, two **explicit** HTML policies

1. **SafeApplicationHtml (implemented):** today's
   `stun_md_render_html`. Escapes untrusted text/code and attributes,
   validates href schemes, rejects unsupported/unsafe content, and preserves
   failure-atomic output. This is the only permitted production HTML mode.
2. **SpecConformanceHtml (future test-only):** proposed explicit API/profile
   walking the **same** owned RichDoc and using separately admitted typed
   raw-HTML blocks/inline content to match official CommonMark examples.
   It must never be selected implicitly or used for untrusted production
   inputs. **Do not build a second Markdown grammar or relax SafeApplicationHtml.**

Before implementing the normative profile, #184 must specify versioned RAW_HTML
node metadata, source and ownership semantics, block-type precedence,
supported entities and embedded control bytes, and separate security tests.
Official #148–191 and #613–632 are not failures of the current basic
TEXT/ATTRIBUTE escaper alone: the parser lacks typed raw-HTML semantics and
the safe policy intentionally encodes markup. The full 652-example survey
must keep these differences visible rather than marking them as silently
passed or weakening the production renderer.

## 5. Qualification and nonregression discipline

- Exact-HEAD installed SDK C11 CTest + ASan/UBSan + real FlexUI/Mesa.
- Strict HTML fixture and owned AST/metadata/failure-atomic gates.
- Full 652 official examples counted; only actual byte-for-byte matches
  enter the immutable nonregression allowlist.
- The root fenced-code fix qualifies #131–133 in addition to the previous
  506 official IDs; the allowlist advances to **509** on the evidence above.
- Unsupported raw HTML, Unicode label folding, images and container Tab
  semantics remain separately tracked. No MarkdownWidget/Tango production
  parser cutover and no compatibility fallback.

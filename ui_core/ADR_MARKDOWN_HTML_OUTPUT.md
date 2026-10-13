# ADR: Markdown whitespace ownership and HTML output profiles

**Status:** Decided for the existing SafeApplicationHtml path; the proposed
SpecConformanceHtml profile and typed RAW_HTML nodes are **not yet implemented**.
**Scope:** Stun C11 Markdown / RichDoc (#181, #184, #172).
**Evidence:** CommonMark 0.31.2, exact HEAD `48432e3`,
[Runtime #38346958887](https://github.com/qigao/stun/actions/runs/38346958887)
(14/14 installed C11 CTests; 247/247 HTML fixtures; 512/652 official exact
matches, 8 unsupported, 132 differences; sanitizers and Mesa green).
Previous root-fence qualification: `df240a0`, Runtime #38344397484.

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
  remaining visual columns must survive as literal spaces within the owned
  CODE TEXT span. Official #5–7 are now qualified by `48432e3` under #181;
  other, more complex nested/tab cases require separate gates. **No trim.**
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
- Root fenced-code #131–133 advanced the qualified set **506→509**;
  list/quote partial-TAB #5–7 now advances it **509→512** based on the exact
  successful Runtime #38346958887 and unchanged official fixture fingerprint.
- Unsupported raw HTML, Unicode label folding, images and unqualified nested
  Tab/container combinations remain separately tracked. No MarkdownWidget/
  Tango production parser cutover and no compatibility fallback.

## 6. Safe URI Autolinks and inline delimiter precedence

As qualified by [Runtime #38352467837](https://github.com/qigao/stun/actions/runs/38352467837)
at source HEAD `2788123`, the C11 inline parser admits bounded HTTP, HTTPS
and MAILTO angle URI autolinks as ordinary owned LINK + literal TEXT nodes.
Their metadata is borrowed only during a live RichDoc lease, while
SafeApplicationHtml remains the sole production renderer and validates every
URL byte before percent/HTML encoding (including URL brackets). URI payload
escapes and HTML entities are **not** recursively parsed as Markdown.

The existing bounded emphasis delimiter-run scanner uses the **same**
Autolink admission predicate to skip the complete opaque URI token; otherwise
external emphasis markers can incorrectly pair with `**` / `__` inside the
URI and reject an entire document. Official #480/#481 and a strict final
LINK-label node-budget rollback now enforce this precedence.

Official newly matching IDs are exactly **#20, #480, #481, #594, #595, #597,
#600 and #603**. Immutable zero-regression gate advances **513→521** on this
independently verified exact-HEAD evidence: Installed SDK C11 14/14, strict
HTML 273/273, native LINK metadata 26/26, full 652-example diagnostic 521
exact / 8 unsupported / 123 different, ASan/UBSan plus Mesa SUCCESS.

CommonMark's arbitrary-scheme autolink grammar is NOT the production-safe URI
policy. Unknown, unapproved or unsafe protocols remain escaped text, not
silently permitted links; RawHTML, explicit XML/XHTML and test-only normative
rendering remain separately scoped. No second Markdown parser or fallback.

## 7. Safe email angle autolinks

The scoped #183 fix 5c48949 passed exact-head Runtime #38354876530: installed
SDK C11 14/14, strict HTML 275/275, owned LINK 28/28, existing
official 521/521 zero regression, full CommonMark 523/652 exact,
8 unsupported and 121 differing, with ASan/UBSan and Mesa SUCCESS.
Official newly matching IDs are exactly #604 and #605; the permanent
byte-exact allowlist is promoted to 523 only on these results.

An admitted angle email is recognized with HTML5 local atoms and bounded
ASCII DNS-style labels by the single C11 inline parser. The same email
predicate shields its punctuation from the existing emphasis delimiter
scanner. The owned LINK href is one contiguous arena metadata span with
an implicit mailto: prefix; the literal label has no such visible prefix.

The production SafeApplicationHtml renderer remains the final URL security
authority. Escaped email local symbols (including \+), malformed domain
labels and unapproved URL schemes remain literal text, not links.
RawHTML, arbitrary-scheme autolinks, explicit XML/XHTML and normative
test-only HTML remain distinct and unimplemented where previously noted.

## Root reference continuation and title whitespace (#183)

The root definition's **opening line** retains its zero-to-three-column
admission limit. Its validated destination/title continuations may have deeper
indentation. The same source interval recorded by the bounded reference map
owns all consumed lines; the block emitter must not reinterpret a continuation
as indented code, a second definition or ordinary text. Root list/quote/fence
admission still uses the existing canonical block predicates.

Destination and title scanners are shared with inline links, not reimplemented
as a second reference grammar. Root titles may span at most **64 physical lines**
including their opener/closer, without a blank or competing root block. Structural
continuation indentation is removed, physical CRLF is normalized to LF, and
payload/trailing spaces remain unchanged. Scratch is bounded by source bytes;
exceeding the line limit fails the entire parse with CAPACITY_EXCEEDED. The
existing 128-entry reference map and 128-raw-byte ASCII label limits remain.

ASCII reference keys now admit punctuation and escaped brackets. They retain
raw escape/entity spelling: `[foo\!]` must not match `[foo!]`, and `[a&amp;b]`
must not match `[a&b]`. Only ASCII case and horizontal whitespace are normalized;
Unicode folding and multiline labels remain separate work.

**HTML5 title and href policies are distinct.** A double-quoted title may safely
contain LF and TAB payload, including decoded character references; quote,
ampersand and angle characters are escaped exactly once. CR and all other
C0/DEL controls remain rejected. The former blanket title-LF negative test is
replaced by an exact owned-metadata/HTML positive plus CR/C0/DEL negatives.
No href policy changed: LF/TAB/CR and unsafe decoded URL schemes still fail
before output. The renderer receives typed metadata and never parses Markdown.

Acceptance includes exact official #193/#194/#196, CRLF/deep indentation,
escape parity, first-definition wins, malformed/blank/competing-block controls,
adjacent definition ownership, raw-input mutation, exact-node-budget rollback,
output-buffer atomicity and line/map/key limits. The permanent 526-ID allowlist
is intentionally not promoted in this source change; full 652-row evidence and
exact-HEAD installed-SDK sanitizers/desktop CI remain required. No production
MarkdownWidget/Tango parser cutover or PR merge is part of this repair.

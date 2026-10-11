# Stun UI C11 semantic core — M1 vertical slice

Tracks [#167](https://github.com/qigao/stun/issues/167); relates to [#90](https://github.com/qigao/stun/issues/90), [#165](https://github.com/qigao/stun/issues/165), [#74](https://github.com/qigao/stun/issues/74), [#15](https://github.com/qigao/stun/issues/15).

This target is **not** a new production layout or Markdown parser. It is the first C11 semantic projection contract that lets the existing FlexUI layout/Markdown implementations and the Tango renderer share native data without extra reflection registries.

## Source of truth

- Runtime state has one application owner: `stun_ui_runtime_state`. Every successful change advances its explicit revision. CMeta reflection reads a **borrowed live instance**, not a snapshot store, and provides no automatic observation or write permission.
- `stun_ui_rect_view` is a nonowning preorder projection **from** the single active layout implementation. Floats are abstract layout coordinates; Tango maps them to terminal rows/columns and gCanvas maps them to glyph/device metrics. This does not calculate Flexbox/Grid yet.
- `stun_ui_document_view` is a nonowning projection **from** the canonical Markdown parser: typed stable IDs, parent indices and exact byte ranges into the original source. It does not tokenize or parse Markdown. Consumers can visit blocks synchronously; asynchronous renderers must take ownership of the required source/IR before the call returns.
- Unicode/Emoji segmentation belongs to the installed `Salts::Unicode` provider; it is intentionally not copied into this IR. Terminal width is a Tango responsibility.
- CMeta immutable metadata and `cmeta_object_ref` do not allocate or retain the native model; provider and module lease lifetime is always managed by an outer owner.

## Why separate reflection from the current C++ ComputedStyle

`flexUI::ComputedStyle` is not a C11 standard-layout record: it contains strings, maps and vectors. It must **not** be cast to a reflected C struct or inspected using fabricated offsets. The existing typed `StylePropertyRegistry` from #90 remains the only CSS type and dirty-impact authority. A later migration can project its resolved primitive values into stable C11 input records.

## Migration checkpoints

- **M1 (this PR):** C11 view-state descriptor + current-value ObjectRef reads; explicit revision invalidation; borrowed flat rectangle/Markdown IR with stable IDs and bounded validation; CMeta lexical cleanup regression, strict C11 compiler and runtime tests.
- **M2:** project existing `md::parse` AST to C11 IR, ensure source lifetime and node parity, then migrate production `MarkdownWidget` consumers without a competing grammar.
- **M3:** replace existing C++ geometry algorithms one semantic slice at a time with C11 measure/arrange, retaining full Flexbox/Grid/media/container test fixtures and **deleting old implementations** as each feature is admitted.
- **M4:** connect C11 geometry/RichDoc to Tango and FlexUI adapters with backend-specific text measurement; validate CJK, grapheme clusters, terminal scrolling and GPU caret shaping.
- **M5:** installed native C11/C++ SDK tests and Windows/macOS/Linux/mobile build gates; no exact package pins or provider fallback.

## How to test the M1 native slice

With the **latest released** Salts SDK installed and in `CMAKE_PREFIX_PATH`:

```sh
cmake -S ui_core -B build/ui-c11 -DBUILD_TESTING=ON
cmake --build build/ui-c11 --parallel
ctest --test-dir build/ui-c11 --output-on-failure
```

A missing `Salts::CMeta` is a configuration failure; no stub or alternative reflection provider is selected. The integration also builds as part of the Stun top-level CMake project. Phase M1 is not complete until executable native CTest passes against an installed Salts provider.

## Native C11 Markdown parser (M2 candidate)

A real Markdown **parser**, not a C++ AST projector, is now available:

- Public API: <stun/markdown_c11.h>
- Entry: stun_md_parse(source, bytes, max_nodes, max_source_bytes, &document)
- Lifetime: stun_md_document_destroy(document)
- Output: stun_md_document_view(document) borrows an owned RichDoc
  with typed block nodes, semantic text spans, metadata, and original bytes.

The parser itself is strict C11, contains no std::string or C++ runtime, and
does not clone Unicode tables. It recognizes ASCII Markdown delimiters while
retaining embedded Unicode text/Emoji bytes. Unicode validity and grapheme
boundaries belong to Salts::Unicode at the surrounding admission/rendering
boundary, not to this initial grammar slice.

**Currently implemented subset**: paragraphs, ATX/Setext headings, thematic
rules, backtick/tilde fenced code blocks, quotes, basic unordered/ordered lists,
GFM pipe tables (matching header/delimiter columns, bounded rows/cells),
emphasis, strong, strikethrough, inline code, basic links/images,
backslash punctuation escaping and line breaks. Code fences retain
exact CRLF/LF bytes and language metadata; links retain target metadata.

**Not yet CommonMark/GFM complete**: reference links, nested/lazy-continuation
list and quote semantics, full GFM table alignment/corner cases,
HTML/entity decoding, footnotes, math,
definition lists, and full delimiter-run/flanking algorithms. Such syntax is
not yet parity-qualified; do not switch all MarkdownWidget callers to C11
until the entire canonical fixture matrix passes.

The existing C++ tools/md parser remains the production semantic authority
during migration. The C11 implementation is a **replacement candidate**, not
a permanent second parsing truth source. Do not parse the same production UI
document through both implementations. Port each feature against the existing
CommonMark/GFM fixtures and retire the corresponding old C++ paths when
end-to-end parity is established. Grammar-tool generation (re2c/Lemon) can
replace current context-aware C11 hand-coded scanning as a single canonical
pipeline, but no second grammar or optional fallback may remain.

The C11 native test stun_markdown_c11_test covers actual parsing, hierarchy,
owned source and semantic spans, stable snapshot identities, metadata,
Unicode/Emoji passthrough, limits, and failure rollback. The installed Salts
SDK test runs it under ASan/UBSan alongside CMeta reflection tests.

The GFM table slice parses escaped pipes and inline-code pipes (including
multi-backtick code spans) without splitting a cell. It admits a table header
only when the next line is a matching delimiter row, accepts pipe-free data
rows, and pads/truncates data cells to the header's column count. A following
blockquote, heading, thematic rule, code fence or list begins a new block.
This is a tested subset, not a complete GFM table conformance claim.

## GFM task list extension (M2 candidate)

Task markers are parsed **only** at the start of a list-item paragraph:
- [x] or [X] produces a checked TaskItem → Checkbox semantic node.
- [ ] produces an unchecked TaskItem → Checkbox semantic node.
- The Checkbox owns no mutable UI state; exactly one byte in its metadata
  range encodes the normalized checked state ('x' or ' ').
- The remainder of the item is still parsed as inline Markdown.
- A marker with following content must be separated by whitespace after ']'.
  Markers inside ordinary paragraphs or escaped markup are not admitted.
- Simple ordered and unordered lists are supported. Nested/lazy task-list
  continuation follows the wider CommonMark block-parsing backlog under #167.

For application interaction, check/uncheck is **not** a parser mutation:
the application keeps the runtime state and performs an explicit source/model
transaction before reparse. CMeta may describe/read that live state without
becoming an implicit writable registry.

## Markdown conformance truthfulness (#172)

The bundled CommonMark 0.31.2 `tools/md/test/spec.txt` contains expected HTML.
The historical `tools/md/test/spec_runner.cpp` checked only that a C++ parse
returned a nonempty root, **never compared expected HTML**, and did not
register its `spec_test_*` executables as normative CTest gates. Its output
now explicitly labels this a parse-only smoke diagnostic rather than
reporting a CommonMark pass percentage.

The C11 `stun_markdown_golden_test` is a separate, executable CTest gate
containing **15 explicitly approved subset fixtures**, verifying every
RichDoc node's kind, parent, heading level, ordinal identity, semantic UTF-8
text bytes, and metadata. The denominator is a checked-in compile-time
constant; missing/extra nodes fail the test. All fixtures run under the
existing installed-Salts ASan/UBSan C11 consumer gate. This is structural
parity for these 15 examples, **not** CommonMark or GFM conformance.

A full spec runner must compare actual rendered HTML against expected HTML
with exact fixture IDs and a complete pass/fail/unsupported denominator,
then expand coverage until the production C++ parser can be retired.

## Independent Python parser oracle (#172)

`ui_core/tests/compare_python_markdown.py` compares **real C11 parser output**
against Mistune 3's independently parsed AST using the CommonMark profile
or the explicit GFM table/task-list/strikethrough plugins. The test invokes
the strict C11 `stun_markdown_ast_dump` executable as a subprocess and reads
hex-encoded RichDoc bytes (not mocked objects or C++ AST). It canonicalizes
known purely representational differences such as adjacent Text segments
and list `block_text` wrappers, but preserves real semantic differences in
heading levels, node hierarchy, links, task checked state, Markdown text,
and table cell alignment metadata.

Every fixture is labeled `gate` or `survey`. **Gated** cases must match
the independent Python AST exactly. Non-gating survey cases have an explicit
denominator and are reported as `DIFF` or `PASS` without contributing to
the gate result; this avoids treating not-yet-supported syntax as success.

Independent `cmarkgfm` bindings render each input to GitHub cmark-gfm
(or CommonMark) HTML and include that reference HTML in the JSON evidence.
The comparison does NOT yet assert C11-generated HTML against this
reference. A future proper HTML renderer/conformance runner must compare
exact expected HTML for the full CommonMark/GFM fixtures.

Important reference limitation: Mistune's GFM table extension may treat
pipes in some backtick code spans as separators even when the intended GFM
semantics differ. `survey.gfm.table-code-pipe` makes that disagreement
reproducible; its cmarkgfm HTML is available for investigation. Do not
silently trust a single Python implementation for disputed GFM syntax.

Executable CI: `unicode-runtime.yml` uses the **installed canonical Salts
SDK** to build the C11 RichDoc dumper, then installs the latest available
Python `mistune` and `cmarkgfm` tools in an isolated test-only virtual
environment without exact package pins. The JSON report and human-readable
comparison log are kept in the normal Unicode CI evidence directory.

Example after building `ui_core` with installed Salts:

```sh
python -m venv .venv-reference
.venv-reference/bin/python -m pip install mistune cmarkgfm
.venv-reference/bin/python ui_core/tests/compare_python_markdown.py \
  --parser build/ui-core-c11-installed/tests/stun_markdown_ast_dump \
  --report build/markdown-python-oracle.json
```

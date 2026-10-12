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

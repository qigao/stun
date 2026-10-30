# Stun UI Markdown forwarding adapter

Status: downstream Draft candidate, not merged, tracked at
https://github.com/qigao/stun/issues/368 .

The canonical parser/semantic RichDoc/official CommonMark/GFM test suite
belongs exclusively to **Salts::MarkdownParser** in SaltsUtils #722 (Draft
PR #724). The separate SAFE/SPEC HTML renderer is **Salts::MarkdownHtml**.

Stun retains `Stun::UICore` and the `stun_md_*` public C API through
`shared/markdown/src/markdown_adapter.c`. It forwards to exactly one
SaltsUtils parser, reusing the same opaque owner/document-view types. There
is no local grammar, HTML writer, fallback parser or duplicate AST.

Stun-specific tests remain in Stun: UI/CMeta/Rect, the public API forwarding
adapter, real Tango and FlexUI MarkdownWidget. Parser-specific C11 tests,
CommonMark 0.31.2 official spec.txt, pinned SAFE/SPEC baselines, cmark-gfm
and Mistune independent oracles now live under SaltsUtils parser/markdown/tests.

Prior to the upstream installed SDK and downstream platform acceptance, a
missing `Salts::MarkdownParser` or `Salts::MarkdownHtml` must fail configure.
Do not merge this candidate or remove the production Stun implementation on
master before exact-HEAD installed SDK consumer proof.

RichDoc node offsets refer to decoded semantic text, not original raw
Markdown bytes. TurboAgent raw-source evidence requires SaltsUtils #725.

# Stun UI Markdown forwarding adapter

The canonical parser, semantic RichDoc model and official CommonMark/GFM
suite belong exclusively to **Salts::MarkdownParser** in SaltsUtils
`parser/markdown`. The separate SAFE/SPEC HTML renderer is
**Salts::MarkdownHtml**. Migration tracking: Stun #368 and SaltsUtils #722/#724.

Stun retains `Stun::UICore` and the `stun_md_*` public C API through
`shared/markdown/src/markdown_adapter.c`. It forwards to exactly one
SaltsUtils parser, reusing the same opaque owner/document-view types.
There is no local grammar, HTML writer, fallback parser or duplicate AST.

## Installed SDK

Stun requires installed `Salts::MarkdownParser` and `Salts::MarkdownHtml`
targets. Missing targets fail configure. Build and install the complete
SaltsUtils package with its documented root Release presets, then select
that installation with `SALTS_UTILS_ROOT` and its matching Salts dependency
with `SALTS_ROOT`. Include the installed Utils and canonical static Crypto
prefixes in `CMAKE_PREFIX_PATH`. An older published package without the Markdown exports
cannot satisfy this dependency.

The existing native CI workflows pin a merged SaltsUtils source commit.
They cache the complete installed SDK by source revision, platform and build
recipe. A missing cache is rebuilt with the official producer root preset
and `cmake --install`; actual Stun targets still link the installed SDK.
No producer Actions run ID or expiring SDK artifact is required. The source
SDK helper is `.github/scripts/restore-flexui-tango-sdk.py`.

## Test ownership

Stun-specific tests remain in Stun: UI/CMeta/Rect, the public C11 forwarding
adapter, real Tango and FlexUI MarkdownWidget. Parser-specific C11 tests,
CommonMark 0.31.2 official `spec.txt`, pinned SAFE/SPEC baselines, cmark-gfm
and Mistune independent oracles live in SaltsUtils `parser/markdown/tests`.

RichDoc node offsets refer to decoded semantic text, not original raw
Markdown bytes. TurboAgent raw-source evidence requires SaltsUtils #725.

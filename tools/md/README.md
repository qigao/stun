# CommonMark conformance fixture

This directory no longer contains a Markdown parser or a build target.
`test/spec.txt` is the unmodified CommonMark 0.31.2, 652-example fixture used
by the canonical C11 RichDoc semantic and HTML gates under
`shared/markdown/tests`.

Production Markdown parsing belongs exclusively to `Stun::UICore` and
`stun_md_parse()`. Do not add parser sources, headers, CMake targets, or a
fallback grammar here.

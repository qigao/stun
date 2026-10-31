# Retired Stun Markdown grammar

All normative CommonMark 0.31.2 fixtures, immutable expected HTML baselines
and parser-generic tests are moved to `qigao/salts-utils/parser/markdown/tests`
under https://github.com/qigao/salts-utils/issues/722.
There is no Stun-local parser, CommonMark spec corpus or fallback implementation.
Stun `stun_md_*` functions are consumer-side forwarders to the one
SaltsUtils C11 parser and renderer.

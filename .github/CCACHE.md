# Native CI ccache

Native focused CI uses [ccache 4.14.2](https://github.com/ccache/ccache/releases/tag/v4.14.2)
on Linux x64, macOS arm64 and Windows x64/MSVC. Our local
`.github/actions/setup-ccache/action.yml` downloads the official
GitHub release with curl and checks its immutable SHA-256 digest.
Unsupported architectures or checksum mismatches fail the job.

`CMAKE_C_COMPILER_LAUNCHER` and `CMAKE_CXX_COMPILER_LAUNCHER`
are set **before** first CMake Ninja configuration, so ccache wraps the
actual compiler, not just upstream package installation. Compiler
content is hashed (`CCACHE_COMPILERCHECK=content`); flags, macros,
headers and source content remain part of the ordinary ccache key.
There is no `CCACHE_SLOPPINESS` or pretend/retry fallback.

Compiler results reside in `runner.temp/stun-ccache/<profile>`.
GitHub cache keys include the pinned ccache version, isolated workflow
profile, OS/architecture and exact source HEAD; prefix restore enables
reuse across later commits. New source/flags/header changes are
invalidated by ccache's own content/command-line hashing. The existing
vcpkg, XML, and SDK caches remain separate.

`report-ccache.sh` emits machine-readable stats (artifact) and a
human-readable job summary. It rejects a seemingly green build which
did not actually invoke ccache for at least one cacheable C/C++
compilation. A **cold** build is allowed zero hits, never zero cacheable
compiles. A subsequent same-platform build should show reusable hits;
measure timings separately before claiming a speedup.

Current rollout: Stun runtime conformance Linux (including separately
instrumented LSan) and FlexUI/Tango native headless Windows/macOS.
These tests still run their existing cross-platform and sanitizer gates.
Other workflows will require separately scoped follow-up activation.

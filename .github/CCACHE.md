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

## First verified cold-run evidence — 2026-10-28

PR #303 initial exact HEAD: `73bc64ced220fb2ec7c3aec5bf63931cba708db0`.

| Native job | Cacheable calls | Hits | Misses | Hit rate | CI |
| --- | ---: | ---: | ---: | ---: | --- |
| Linux / installed SDK + ASan/UBSan/LSan | 611 | 27 | 584 | 4.42% | [40673715901](https://github.com/qigao/stun/actions/runs/40673715901) |
| macOS / FlexUI Tango Headless | 825 | 36 | 789 | 4.36% | [40673715890](https://github.com/qigao/stun/actions/runs/40673715890) |
| Windows / real MSVC + ConPTY | 327 | 0 | 327 | 0.00% | [40673715890](https://github.com/qigao/stun/actions/runs/40673715890) |

All three native cacheable-invocation gates passed, their archived
compiler-output caches were saved with immutable `stun-ccache-v1-4.14.2`
keys, and all original native/sanitizer tests passed. A zero-hit cold
Windows cache is legitimate; no warm-cache saving has been claimed.

A second exact-head run created from this documentation change should
restore the first commit's cache snapshot via the platform/profile
restore-prefix, without changing any C/C++ source or compilation options.
It must still run the identical real tests and report independently
verified ccache hit/miss stats. Do not infer a speedup until the restored
cache and timings are observed.

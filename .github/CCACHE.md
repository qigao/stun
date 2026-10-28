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

## Warm-cache proof and CI change-scope gate — 2026-10-28

Second HEAD `f45f2d8c815d5d9d24dfed13c163dc93d42d1338`
([run 40675278914](https://github.com/qigao/stun/actions/runs/40675278914))
restored the prior exact-HEAD macOS ccache cache snapshot via
`restore-keys`: **283 cacheable compilations, 283 hits, 0 misses
(100% hit rate)**, with the same real TUI/PTY and native tests passing.
This is independently distinct from the XML/vcpkg cache restoration.

The previous second-run Linux runtime scope was **skipped** because a
documentation-only push does not match the deliberate
`.github/scripts/ci-affected.sh` latest-push filters. A skipped job is
NOT a Linux warm-cache qualification. Changes to the shared
`setup-ccache` Action and stats reporter must now select the native XML
runtime gate, and the Windows/macOS Headless workflow path filter includes
them too. This workflow change triggers a fresh Linux full native build,
allowing the first run's Linux compiler-output snapshot to prove restore
and recompile correctness. Continue to evaluate the real exact-HEAD stats;
do not count a skip as proof.

## GraphLayout 4-way compiler-cache rollout (Draft follow-up)

GraphLayout's former GCC/Clang build used independently installed, mutable
ccache packages plus a source-path hash; Windows Visual Studio compilation
had no compiler cache. The dedicated GraphLayout CI follow-up reuses the
same SHA-256-pinned ccache 4.14.2 Action and machine-readable stats gate
on four isolated profiles: Linux GCC, Linux Clang, macOS AppleClang,
and Windows MSVC. Windows switches the build **generator** to Ninja under
the real x64 MSVC environment; this is required to honor CMake compiler
launchers and must pass the actual GraphLayout regression suite. There is
no fallback generator or compiler on a failed cache installation.

All four entries compile the real independent GraphLayout libraries in
Release, require nonzero ccache invocations, run CTest and archive exact
source-HEAD stats. The full 4-way exact-head CI is mandatory before
claiming the follow-up accepted or merging it.

## GraphLayout CI first-run macOS Bash 3.2 failure and fix

Initial draft HEAD `54d1622b` compiled and tested Linux GCC/Clang and
entered Windows real MSVC/Ninja, but macOS failed *before configuring*
CMake: Bash 3.2 with `set -u` reports an unbound variable when the
optional empty `compiler_args[@]` is expanded. This is a script-level
portability error, not a failed GraphLayout algorithm or a ccache miss.
The fixed command uses a mandatory nonempty `cmake_args` array with
optional toolchain overrides appended; strict shell error handling is
unchanged and the exact four-platform CI must pass at the new HEAD.

## GraphLayout first all-four-way green and cache evidence (2026-10-28)

PR #304 exact accepted first full HEAD `d5ebd3f6977aa956a2a03c75722ce46085d685bf`
([run 40698224003](https://github.com/qigao/stun/actions/runs/40698224003))
passed all real GraphLayout CTest suites in four toolchains, including
Windows Ninja + MSVC and pinned SHA-256 checked ccache:

| Compiler | Cacheable calls | Hits | Misses | Classification |
| --- | ---: | ---: | ---: | --- |
| Linux GCC | 45 | 45 | 0 | Restored previous commit's GCC cache |
| Linux Clang | 45 | 45 | 0 | Restored previous commit's Clang cache |
| Windows MSVC | 44 | 44 | 0 | Restored previous commit's MSVC cache |
| macOS AppleClang | 45 | 0 | 45 | First successful build: cold cache saved |

The immediately previous HEAD `54d1622b` had already passed Linux
GCC/Clang and Windows MSVC but failed before macOS CMake configuration
because Bash 3.2 `set -u` rejected an empty optional array.
A later no-C++-source-change CI invocation must restore the AppleClang
snapshot, report real warm compiler hits and rerun native tests before
we regard AppleClang cache reuse as qualified. This is a separate
acceptance criterion, not a claim inferred from the first save.

## Tango Native C Core follow-up — independent rollout

The independent Tango C11 CI builds against the real checksum-qualified
published Salts SDK on Linux, macOS and Windows and separately runs
Linux ASan/UBSan + TSan acceptance. Its dedicated CI follow-up shares
the already merged, verified ccache 4.14.2 Action and the real
compiler-invocation stats gate, using a new isolated `tango-core`
cache profile.

All three OSes use Ninja to honor both C and C++ CMake compiler
launchers. Windows retains the real MSVC toolchain (activated via
`ilammy/msvc-dev-cmd`) and links the existing C11 and C++17 tests; it
is **not** a fallback/compiler substitution. Linux ASan/UBSan and
TSan flags remain unchanged and ccache keys include sanitizer flags.
The original CTest gates are not skipped or relaxed.

Accept only after the independent Tango workflow passes on an exact
source HEAD for all three platforms and reports nonzero cacheable
compilation calls; cold and warm-run hit rates must be distinguished.

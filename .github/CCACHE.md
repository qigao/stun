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

## Tango Windows/MSVC Debug compiler-cache rejection (2026-10-28)

First Tango CI head `da5ad9a1` completed its real C11/C++17 Windows
native tests, but the ccache gate correctly rejected **0 cacheable
compiles out of 22 invocations**. We examined the unmodified
`tango-native-Windows-X64` workflow TSV: the only rejection reason was
`unsupported_compiler_option=22`, not a failure to install ccache or
run the actual compiler. MSVC's default Ninja Debug flags use `/Zi`,
which writes compiler PDB files and is not ccache-cacheable.

The follow-up explicitly configures **Windows only** with
`CMAKE_C_FLAGS_DEBUG` and `CMAKE_CXX_FLAGS_DEBUG` set to
`/MDd /Z7 /Ob0 /Od /RTC1`. This retains MSVC's Debug CRT, runtime
checks, unoptimized compilation and debug symbols; `/Z7` embeds
symbols in each object instead of an independently mutable compiler
PDB. Linux/macOS compiler flags and independent ASan/UBSan/TSan builds
are untouched. The actual Windows cacheable compiler calls must become
nonzero under the unchanged strict stats gate, followed by warm-cache
acceptance. No CI failure is waived.

## Tango Windows Git Bash path conversion in /Z7 CI override

On the first `/Z7` candidate, CMake's Windows compiler ABI test
failed BEFORE testing Tango: Git Bash's MSYS argument translation
rewrote `/MDd` into `C:/Program Files/Git/MDd`, which MSVC interpreted
as source-file arguments. The command-line debug configuration itself
was correct, but the shell modified it.

Windows standalone `cmake` now runs with `MSYS_NO_PATHCONV=1` only
for that configure subprocess. The project source/build arguments are
relative and the installed SDK prefix was explicitly converted with
`cygpath -w` already; native `/MDd /Z7 /Ob0 /Od /RTC1` flags thus
reach CMake unchanged. Linux/macOS config and sanitizers are untouched.
The existing mandatory ccache stats gate will prove whether the Debug
MSVC object compilation became truly cacheable.

## Tango exact-HEAD native compiler-cache qualification (2026-10-28)

PR #305 first ALL-green exact HEAD
`410a43e934c9984111f1e770d7239439bdce24fd`:
[Tango native CI #40700973600](https://github.com/qigao/stun/actions/runs/40700973600)
is SUCCESS on Linux, macOS, Windows, including Linux's real
ASan/UBSan and TSan work.

| Platform | Cacheable calls | Hits | Misses | Classification |
| --- | ---: | ---: | ---: | --- |
| Linux / real ASan+UBSan+TSan | 53 | 51 | 2 | Restored a previous head; 96.23% |
| macOS / native C11+C++ | 19 | 19 | 0 | Restored a previous head; 100% |
| Windows / real MSVC /Z7 Debug | 21 | 0 | 21 | First cacheable Windows build: cold output saved |

On Windows the independent MSVC executable/CTest gates really passed,
and the previous `unsupported_compiler_option=22` disappeared. Still,
a saved cache is not a *restored* cache: Windows warm reuse must be
verified separately before final acceptance.

The follow-up adds a PR-scoped concurrency group to prevent overlapping
native builds from publishing stale evidence when new exact-HEAD
updates arrive. Because this commit does not modify C/C++ source or
compiler flags, it also provides a legitimate three-platform warm-run
validation. The Windows cache must show nonzero real hits and all
original OS/sanitizer checks must pass; success/skipped are distinct.

## Standalone Markdown highlighter loader rollout — Draft CI follow-up

The isolated C11 dynamic provider and C++17 highlighter loader build/test
on Linux, macOS and Windows; an independent Linux TSan job compiles and
tests BOTH the provider and loader with ThreadSanitizer enabled. This
workflow now uses the repository-owned checksum-verified ccache 4.14.2
Action and the strict real compiler-invocation stats reporter.

Native matrix CMake uses Ninja across all OSes, including Windows with
the real MSVC toolchain. Compiler-output profiles are `markdown-loader`
and `markdown-loader-tsan`, so sanitizer flags and compiler identities
cannot be confused with unsanitized local results. No TSan warning,
native DSO unload test or compiler-cache validation is skipped. All
four exact-HEAD jobs, cacheable-invocation statistics and binary test
logs are required before declaring the follow-up accepted.

## 2026-10-28 Markdown highlighter Windows native cache RED

The first draft head `8bb0d061` passed the actual Windows C11
provider/C++17 DSO loader build and CTest, but the **strict ccache**
gate correctly failed: 10 compiler invocations, 0 cacheable.
The installed pinned ccache executable was active; the suspected
cause is MSVC's default Ninja Debug `/Zi` compiler-PDB option,
the same unsupported configuration diagnosed and repaired in Tango
#305. The Windows-only candidate uses explicit `/MDd /Z7 /Ob0 /Od
/RTC1` in both C and CXX Debug CMake flags. It preserves debug
symbols in object files and the existing real compiler and tests.
`MSYS_NO_PATHCONV=1` only on the Windows CMake subprocess preserves
MSVC flags under Git Bash. No CI error/TSan diagnostic is suppressed.

This candidate rebases onto the actual merged `master` commit
`be2a9f22` to retain Tango's verified cache rollout and appends
this standalone loader content AFTER that master document. Windows
must report nonzero actual cacheable compiler calls before passing.

## Markdown highlighter first Windows cacheable MSVC build (2026-10-28)

PR #306 exact corrected head `afe2e302`
([run 40708681421](https://github.com/qigao/stun/actions/runs/40708681421))
passed ALL 4 native jobs including C11 DLL + C++17 DSO loader
conformance and the independent Linux ThreadSanitizer test:

| Job | Cacheable compilations | Hits | Misses | Result |
| --- | ---: | ---: | ---: | --- |
| Linux loader | 8 | 8 | 0 | PASS, prior cache restored |
| Linux independent TSan | 8 | 8 | 0 | PASS, prior cache restored |
| macOS AppleClang | 8 | 8 | 0 | PASS, prior cache restored |
| Windows x64 MSVC Debug /Z7 | 9 | 0 | 9 | PASS, cold cache saved |

Before that fix, Windows DSO tests passed but the cache gate rejected
**zero cacheable compilations / 10 calls**, proving that a normal
CTest pass cannot replace compiler cache evidence. The new
Windows `/Z7` debug setting yields actual cacheable MSVC C11/C++17
commands, not a substituted compiler.

To guard against a future accidental `/Zi` regression, the subsequent
candidate exports Ninja's actual C/C++ compile commands and requires
Windows `/Z7` present with `/Zi` absent. This also triggers a second
same-source-head CI run, whose Windows result must prove REAL restored
compiler cache hits rather than treating the first save as a warm run.

## UI Core C11 released-SDK native qualification

`.github/workflows/ui-core-c11.yml` now uses the existing SHA-256
verified `setup-ccache` action for **20 original native C11 tests** on
Windows x64/MSVC (Ninja generator, genuine Visual C compiler) and macOS
ARM64/AppleClang (Ninja). Both generated `build.ninja` graphs must
actually contain the `ccache` compiler launcher before any build is
considered valid. Original Salts.Native released SDK provenance,
SHA-256 validation, and 20/20 CTests are unchanged.

The `-fsyntax-only` C11 header/consumer jobs deliberately remain
independent of ccache: they produce no cacheable object output and
must not be reported as compiler-cache evidence. Isolated output cache
profiles (`ui-core-c11-windows`, `ui-core-c11-macos`) avoid mixing
native build configurations with FlexUI/Tango/GraphLayout.

Changes to `setup-ccache/**` and `report-ccache.sh` trigger both the
workflow and its `ci-affected.sh` native-scope check. The actual
compiler invocation/hit/miss counts, 20 CTests and Ninja
`compile_commands.json` are kept as exact-head CI evidence.
No warm-cache benefit or speedup is claimed until independently
measured on another exact SHA.

## UI Core C11 first verified cold-run evidence (PR #307)

On immutable candidate `0eaca57a1d74051cf72c8e59045f1cdc95e9be58`,
[UI C11 Reflection Contract #40716771115](https://github.com/qigao/stun/actions/runs/40716771115)
completed successfully, including both original released-SDK 20/20 native
CTest suites, independent strict syntax-only header tests and the normal
public/runtime contract workflows.

| Target | Cacheable compiler calls | Hits | Misses | Other calls |
| --- | ---: | ---: | ---: | ---: |
| macOS ARM64 AppleClang Ninja | 28 | 0 | 28 | 0 |
| Windows x64 MSVC Ninja | 26 | 0 | 26 | 4 |

Both compiler caches were saved to immutable, separate ccache 4.14.2
keys. Cold zero-hit statistics are expected. The next workflow-only
comment commit intentionally preserves the production C sources and
CMake compiler configuration, forcing real native jobs to restore
the prior cache by exact platform/profile prefix; confirm restored
cache hit statistics instead of relying on the presence of archives.

## gCanvas native GLES and Android gates

The active upstream qualifications test **production gCanvas targets**, not
synthetic installed-package consumer projects: the Linux
`gcanvas-gles-runtime.yml` compiles/executes the real OpenGLES renderer and
pixel test with verified ccache, while `gcanvas-android-egl.yml` compiles the
actual Android EGL adapter, and `gcanvas-android-runtime.yml` runs the real
Android Surface replacement application. Each workflow retains exact-head and
native runtime rules; consumer-only CI and generated `build/consumer` CMake
projects are forbidden.

The former `gcanvas-package-contract.yml` combined a synthetic
`find_package` consumer with a separate NDK-default configuration job. It
was retired rather than allowed to remain a required synthetic consumer gate.
The older [PR #308](https://github.com/qigao/stun/pull/308) installed-consumer,
NDK and cache-hit observations are **historical evidence only**. The current
merge acceptance derives from the surviving native GLES/Android jobs, and real
existing downstream applications/libraries may add separate SDK-use evidence
without creating test-only consumer projects or additional CI workflows.

## Completed release qualification (2026-10-28)

These are verified compiler-output hit counts from real native build jobs,
not GitHub cache-restoration messages or measured wall-time speedups.

| Accepted PR | Exact HEAD | Real cacheable calls / hits | Acceptance |
| --- | --- | --- | --- |
| [#307](https://github.com/qigao/stun/pull/307) Windows x64/MSVC C11 | `21a0e10f` | 26 / 26 (100%) | 20/20 native CTests |
| [#307](https://github.com/qigao/stun/pull/307) macOS ARM64 C11 | `21a0e10f` | 28 / 28 (100%) | 20/20 native CTests |
| [#308](https://github.com/qigao/stun/pull/308) Linux OpenGLES | `10b7f48a` | 17 / 17 (100%) | GLES pixel smoke |
| [#308](https://github.com/qigao/stun/pull/308) Linux installed consumer | `10b7f48a` | 17 / 17 (100%) | `find_package(gCanvas)` |
| [#308](https://github.com/qigao/stun/pull/308) Android NDK ARM64 | `10b7f48a` | 14 / 16 (87.5%) | NDK target/ABI checks |

Full workflows: [UI C11 #40717285861](https://github.com/qigao/stun/actions/runs/40717285861),
[GLES #40719262944](https://github.com/qigao/stun/actions/runs/40719262944),
[package/NDK #40719262892](https://github.com/qigao/stun/actions/runs/40719262892).
Cold snapshots and restored warm snapshots use unchanged production sources
and compiler flags. The two NDK misses are legitimate, not test failures.
No controlled wall-time benchmark has been run.

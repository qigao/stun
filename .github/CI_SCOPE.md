# Stun CI: incremental scope and complete requalification


## Master protection and obsolete acceptance status (2026-11-02)

The [master ruleset #26090409](https://github.com/qigao/stun/rules/26090409)
was active when this cleanup was prepared. It required pull requests and a
separate `Stun required acceptance` status from GitHub Actions, with no bypass
actors. **That status publisher is retired by this change:** it did not compile
or execute any native tests; it only republished other workflow conclusions.

The `Stun required acceptance` required-status context (integration ID 15368)
was removed from the server-side ruleset on 2026-11-02. This change
enabled [PR #399](https://github.com/qigao/stun/pull/399) to merge at
`3cdf44d7d7d011975beae4696258fe8d78266338`. The master pull-request,
non-fast-forward and branch-deletion protections remain in place. Git commits
do **not** modify server-side rulesets.

Qualification must use the **actual native build, CTest, runtime, sanitizer,
graphics and platform jobs on the precise source HEAD**, not a synthetic
publisher. A skipped/cancelled or obsolete-head result is not executed
acceptance, even if the workflow summary appears green. Review each
applicable real job before merging; do not replace the publisher with another
no-op acceptance/check-mark workflow.

## Why the old CI was expensive

GitHub applies `pull_request.paths` to the **entire PR diff**, not just the
newest pushed changes. A long-lived PR that has touched graphics, C11 Markdown,
GraphLayout and Mermaid consequently launched unrelated Android, GLES and
installed-SDK jobs after every small parser commit. Two workflows also listened
to both unfiltered branch pushes and PR updates, which could produce duplicate
work on feature branches.

## What the current checks mean

Each affected-component workflow starts with a small `changes` job. On a PR
`synchronize` event it compares the webhook's `before` and `after` commits;
only jobs whose files changed in that update run. On a `master` push the
classifier also compares the event's exact `before` and `after` commits:
only source-owning native/SDK gates execute, including for multi-commit pushes
or squash merges. It checks both sides of renamed/moved files. If Git
history, event metadata, checkout identity or the comparison is unavailable,
the check **runs**, rather than silently being skipped.

`workflow_dispatch` and PR `ci/full` remain explicit full-qualification
overrides. The fast repository-wide parser architecture check remains required
on every PR and master push.

The rapid-iteration path is:

| New change | Actual heavy validation |
| --- | --- |
| Flowchart lexer/parser-only update | Real Lemon/re2c AST smoke plus an independent **published-SDK JSON golden** lane, both with ASan/UBSan; full Mesa/FlexUI waits until `ci/full` |
| Flowchart JSON golden/test or serializer update | Published-SDK JSON golden lane; serializer/integration changes also run full runtime |
| GraphLayout algorithms | GraphLayout native matrix and runtime |
| gCanvas renderer or Android platform | Relevant GLES/Android/package/cache checks and runtime |
| C11 Markdown | UI C11 released-SDK matrix and runtime |
| Controller/provider or public dependency boundary | Actual native runtime / installed-SDK tests |
| Documentation only | Fast architecture check; unrelated native/SDK runs are skipped |

Matrix and sanitizer coverage has **not** been weakened, and the integration
tests and golden fixtures have not been altered by this CI refactor.

The `Flowchart native AST smoke` check compiles the *existing* C11 parser,
repository Lemon template and re2c lexer in an isolated CMake project. It
does not link the SaltsUtils JSON serializer, FlexUI or renderer, and does not
establish Mermaid golden JSON conformance. The lightweight runner uses Ubuntu's
re2c. The test-only `STUN_FLOWCHART_PARSER_ONLY` define removes only JSON
serialization from the standalone smoke build; production retains it.

The separate `golden` job inside the existing `Stun runtime conformance`
workflow uses verified Salts.Native and SaltsUtils.Native NuGet archives,
official GitHub Release SHA256SUMS, canonical Core SONAME aliases and the
original 95-case Flowchart `test_parser.c` suite. It now builds and runs both
the production JSON golden target and the lightweight native AST smoke in
one isolated CMake build tree, **without** Mesa, Android emulation, the
FlexUI renderer, or an alternate JSON provider. This eliminates a duplicate
PR runner while preserving independent AST and JSON assertions.
The `Flowchart native AST smoke` workflow is now a manual-only diagnostic
entry point; its CMake harness works locally without the published SDK.

The `golden` job is a conformance check on its own, not a replacement for
complete released-SDK/FlexUI/Mesa integration on the final exact HEAD.

The `ci-affected.sh` helper accepts a leading `!` to exclude specific
parser-only paths from the expensive runtime gate. When `ci/full` is set,
`ci-affected.sh` compares the **entire PR base-to-head diff** using the same
conservative, path-scoped M1 documentation classifier. If and only if the
diff is unambiguously documentation-only, it skips unrelated native suites;
the existing Runtime change-scope job still exercises CI routing policy tests.
For any changed source, missing history, invalid path, or unavailable policy,
`ci/full` still forces all applicable real native suites (fail open).
No fixture in a source-owned tree is discarded because of a `.md` suffix. Mixed commits that also
touch integration files still run the full check; invalid event metadata,
missing history and missing include patterns fail open. All exclusions are
ignored when `ci/full` is present.


## Before merging a PR

Use the `ci/full` label to request all applicable **real** qualification
jobs for the final source HEAD. Prefer setting it before the final source push
so the existing native workflows rerun once on that exact commit; a label is
not itself test evidence. Do not add fake status jobs, redundant workflow
launches or no-op commits to coerce an old green result into acceptance.

Read the actual GitHub Actions **per-job** results: for a source change,
require successful executed native CTests and affected real platform/SDK
suites on the exact HEAD. `skipped`, `neutral`, `cancelled`, pending,
missing, failed or old-SHA jobs cannot be reported as native acceptance.
Documentation-only updates may correctly bypass unrelated heavy native
suites while retaining existing repository policy tests.

The obsolete aggregated commit status is no longer used. Until the
master ruleset has been edited to remove it, GitHub may still block merges;
that is a server-side configuration issue, not a reason to restore a
status-only publisher.

## Isolate unrelated PR labels from real CI execution

The eight active workflows listening for `labeled` events accept `ci/full` as an
explicit same-HEAD requalification. Any other label must not cancel a
queued/running check or claim a green test. For unrelated label events,
workflow concurrency uses a unique `github.run_id` suffix, and `changes`
is skipped before a runner is allocated. Normal `opened`, `synchronize`,
`reopened`, `push`, and `ci/full` keep the original shared per-PR or
per-ref `qualification` concurrency group, so new source heads can still
cancel obsolete executions.

The existing classifier also ignores unrelated label events (defense in
depth). `test_ci_affected.py` statically checks all nine label-sensitive
workflow guards and concurrency groups. A skipped label event must never
be presented as an executed native test.

## Isolate manual full qualification from master push CI

The `runtime-conformance` and `graphlayout-native` workflows listen to
both `push` on `master` and `workflow_dispatch`. Previously a manual
run on `master` shared the same `qualification` concurrency key as a
push run; either event could cancel the other's in-flight tests, even
though one is explicit full qualification and the other is automatic
incremental validation.

A `workflow_dispatch` now uses a unique `github.run_id` group suffix
for these two workflows. Unrelated labels retain their own isolated
groups; ordinary master pushes, PR updates, and explicit `ci/full`
events retain normal superseding semantics. Test commands, module scope
filters, and required source coverage are unchanged.


## Optional exact-HEAD evidence reader (not CI)

`.github/scripts/verify_exact_head_ci.py` is a **read-only** manual tool for
querying live GitHub PRs, workflow runs and executed jobs. It rejects
skipped, missing, cancelled or stale-head evidence. Its trusted module-path
matrix comes from `.github/scripts/ci_required_matrix.py`. These scripts do
not run native builds, publish GitHub statuses or enforce master merges.

For manual diagnosis only:

```sh
GH_TOKEN=... python3 .github/scripts/verify_exact_head_ci.py \
  --repo qigao/stun --pr PR_NUMBER
```

The offline policy/verification tests remain part of the existing Python
CI-scope test run; they are not independent native acceptance workflows or
substitutes for production CMake/CTest evidence.

## Fail-fast CI/CMake policy (2026-10-30)

**No detect-then-build probes.** CI should run real CMake configure, native
build/link and actual CTest/application behavior. Do not add shell
`grep`/file-existence/loaded-library/compile-command-text inspections, manual
CMake header parsing, tool/package version sniffing, warnings followed by
silent fallback, or synthetic installer/consumer projects and CI. A missing
requested compiler, generator, imported target, resource or ABI must produce
a failed REQUIRED configuration, compiler error, linker error or real test.

Native semantic assertions stay in **tests**, including real terminal, GUI
screenshots, shader/pixel, Android and lifecycle behavior; explicit product
feature choices, trusted exact-head CI routing and release checksums are not
substitutes for a build, but remain required for provenance. Compiler caches
remain accelerators; report actual invocations, not source/manifest greps.
Producer installation/export APIs remain part of the library; verify SDK
integration using an **existing real downstream application/library** and its
ordinary CI, not a dedicated consumer/installer fixture.

The existing sole public CI policy job continues to execute real Python
source-routing tests. It no longer duplicates static CMake/provider scans or
compile-only pseudo-consumers that cannot establish runtime correctness.


## CI source routing and read-only helper policy

The existing `.github/scripts/ci-affected.sh` compares the exact source
delta and schedules real workflows only for affected subsystems. For
`ci/full` on a clearly documentation-only PR, its existing
`.github/scripts/ci_required_matrix.py --doc-only-z` path prevents
unrelated heavyweight native jobs. Unavailable or ambiguous evidence
remains fail-open toward real testing. `ci_required_matrix.py` also assists
the optional read-only exact-head audit above; it no longer feeds a status
publisher.

The old `Public dependency compile contract` workflow did not compile a
library or execute a production parser; it only launched Python CI-routing
policy tests. It has been deleted. The exact same `test_ci_affected.py`
and its existing policy/screenshot-oracle unit fixtures now execute **once**
inside the already-running `Runtime change scope` job in
`Stun runtime conformance`. No extra job or runner is created.
This scope test is never counted as native runtime acceptance: real
CMake/CTest/SDK coverage is supplied by the existing focused native jobs,
`Stun native GUI screenshot E2E`, and relevant platform workflows.

## Retired status-only workflow

Deleted: `.github/workflows/required-acceptance.yml`,
`.github/scripts/publish_required_acceptance.py`, and its dedicated
Python publisher fixture. This removes the privileged
`pull_request_target` / `workflow_run` fan-out and the
`statuses: write` token that previously posted `Stun required acceptance`.

The old required-status entry was independently removed from
[ruleset #26090409](https://github.com/qigao/stun/rules/26090409)
before merging #399. [Issue #315](https://github.com/qigao/stun/issues/315)
records the earlier system and its retirement. CI acceptance claims must
cite executed native workflow/job results directly, never an aggregated
synthetic status.

## Maintenance

- Add new files/paths to both the top-level trigger and
  `.github/scripts/ci-affected.sh` invocation when expanding subsystem scope.
- Keep `push` for shared path-specific workflows restricted to `master`, so
  a feature branch with an open PR does not duplicate every job.
- Run PR qualification for `opened`, `synchronize`, and `reopened`; retain
  `labeled` solely for explicit `ci/full` requalification. Do **not** schedule
  duplicate full suites on `ready_for_review`: the PR head SHA and source did
  not change, and draft PRs have already run their normal checks. Use `ci/full`
  when a same-HEAD requalification is explicitly needed.
- Keep `concurrency.cancel-in-progress` for obsolete results but never
  interpret cancellation as test success.
- Keep full runtime integration, independent published-SDK tests, sanitizers,
  Android emulator coverage and graphics pixel tests available for changes to
  their owning subsystems and for pre-merge requalification.
- Validate routing behavior with
  `python3 .github/scripts/tests/test_ci_affected.py`.
- Run the native parser smoke locally with
  `cmake -S charts/mermaid/flowchart/tests/standalone -B build/flowchart-ast-fast`,
  `cmake --build build/flowchart-ast-fast`, and
  `ctest --test-dir build/flowchart-ast-fast --output-on-failure` (re2c is
  required). This does not replace the full exact-HEAD JSON golden gate.

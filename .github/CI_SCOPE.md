# Stun CI: incremental scope and complete requalification

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
| Controller/provider or public dependency boundary | Public provider contracts and runtime |
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
parser-only paths from the expensive runtime gate. Mixed commits that also
touch integration files still run the full check; invalid event metadata,
missing history and missing include patterns fail open. All exclusions are
ignored when `ci/full` is present.

## Before merging a PR

Add the `ci/full` PR label (create the repository label if necessary) and keep
it on the PR. The `labeled` event triggers a complete requalification of all
workflows applicable to the cumulative PR diff; subsequent `synchronize`
events with the label run those checks for each new HEAD. Removing an unrelated
label never requests full requalification.

Do not merge unless **the exact HEAD** has a completed, successful *executed*
Stun runtime conformance job with chart JSON CTests, plus the relevant platform,
installed-SDK and lifecycle jobs. An old-head success or a skipped job is not
equivalent. The usual full runtime workflow can also be started with
`workflow_dispatch` on a selected branch, but use the `ci/full` label for an
entire PR qualification matrix.

**GitHub branch protection caveat:** a job skipped by `if:` can appear green.
A green workflow name alone does not prove its runtime tests executed. Keep
the exact-head full qualification as a mandatory release/merge review rule;
if repository rulesets need automated enforcement, require a dedicated
non-skippable full-acceptance check rather than treating `skipped` as passed.

## Isolate unrelated PR labels from real CI execution

The nine workflows listening for `labeled` events accept `ci/full` as an
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

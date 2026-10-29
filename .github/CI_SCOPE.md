# Stun CI: incremental scope and complete requalification

## Enforced master rules (2026-10-29)

The active [master exact-HEAD acceptance ruleset](https://github.com/qigao/stun/rules/26090409)
targets only `refs/heads/master`. Every change must go through a pull request
and pass the single `Stun required acceptance` commit-status context from
GitHub Actions (integration ID 15368). There are no bypass actors, including
repository administrators. Direct pushes, force pushes and branch deletion
are blocked by the applicable rules.

Pull requests require zero approvals; the exact-head acceptance gate remains
mandatory. Existing merge, squash and rebase methods remain available.
The ruleset does not require an up-to-date test-merge SHA and does not enable
a merge queue: the trusted publisher qualifies the actual PR source HEAD,
with `ci/full` and executed jobs selected from the cumulative source delta.
Documentation-only PRs still require the unfiltered parser contract, without
unrelated native platform matrices.

The M0/M1 sections below describe the earlier implementation stages. The
repository rules API, not those historical stage limitations, is the source
of truth for current enforcement. See [issue #315](https://github.com/qigao/stun/issues/315)
for the activation and live negative/positive verification requirements.

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
parser-only paths from the expensive runtime gate. When `ci/full` is set,
`ci-affected.sh` compares the **entire PR base-to-head diff** using the same
conservative, path-scoped M1 documentation classifier. If and only if the
diff is unambiguously documentation-only, it skips unrelated native suites;
the repository-wide parser contract still executes and remains required.
For any changed source, missing history, invalid path, or unavailable policy,
`ci/full` still forces all applicable real native suites (fail open).
No fixture in a source-owned tree is discarded because of a `.md` suffix. Mixed commits that also
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

**Required-status distinction:** a job skipped by `if:` can appear green.
A green workflow name alone does not prove its runtime tests executed. The
active master ruleset therefore requires only the dedicated
`Stun required acceptance` commit status. Its trusted verifier rejects
missing, skipped, neutral, cancelled, stale-head or failing required jobs.

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

## Exact-head evidence verifier (stage M0; not a branch rule)

`.github/scripts/verify_exact_head_ci.py` can read GitHub's live PR,
workflow-run and per-job REST records and fail closed unless all explicitly
required jobs ran successfully on the **current exact PR head**. It rejects
`skipped`, `neutral`, `cancelled`, pending, missing and stale-head jobs.
It verifies that `ci/full` is present and rereads the PR head after fetching
evidence. Requirement files list trusted workflow paths and exact job names;
they **must be produced from reviewed module ownership**, never accepted
from untrusted PR input.

Example requirement JSON for a runtime-affecting change:

```json
{"required_workflows": [
  {"path": ".github/workflows/public-dependency-contract.yml",
   "jobs": ["Repository sole C11 parser contract"]},
  {"path": ".github/workflows/runtime-conformance.yml",
   "jobs": ["Linux / focused installed-SDK ASan/UBSan"]}
]}
```

Run after the relevant `ci/full` suites finish:

```sh
GH_TOKEN=... python3 .github/scripts/verify_exact_head_ci.py \
  --repo qigao/stun --pr PR_NUMBER --requirements reviewed-matrix.json
```

The deterministic positive/negative cases run inside the existing public
dependency parser-check job through `test_ci_affected.py`. This is **not**
yet a non-skippable required PR check, does not protect `master`, and
does not itself infer the module matrix. See [Issue #315](https://github.com/qigao/stun/issues/315)
for the required always-created GitHub status and administrator-enabled
ruleset. Do not merge based solely on this validator or a green workflow
summary.

## Trusted source-delta CI requirement matrix (stage M1)

The default-branch `.github/scripts/ci_required_matrix.py` now derives a
non-empty set of required native jobs from the actual PR changed-file list
(including both sides of renames) and the **trusted default branch's**
`pull_request.paths` routing. Exact native job names are pinned in source
and updated alongside their owning workflows. This module must never be loaded
from the PR head during a privileged publication workflow.

The unfiltered Public Dependency parser contract is always required.
Non-documentation changes require real Linux installed-SDK Runtime
Conformance; affected focused workflows add Windows/macOS C11, GraphLayout,
GLES/NDK, native Android emulator, TSan, Tango and other exact host gates as
applicable. Documentation-only changes do not require irrelevant native
matrices. Documentation classification is **path-scoped**, not extension-only:
root documentation, the dedicated `docs/` and `documentation/` trees and the
explicit `.github/CCACHE.md` / `.github/CI_SCOPE.md` policy documents are
recognized. A `*.md` test fixture inside `shared/markdown/`, `charts/`,
`flexUI/`, or any other source module still requires actual native acceptance.
Missing/renamed workflow evidence, malformed paths, unknown source
roots, empty PR deltas and truncated API pages fail closed rather than
succeed with a reduced gate.

`verify_exact_head_ci.py` now uses this trusted policy automatically
(without `--requirements`); the explicit manifest option remains a manual
diagnostic mode only. `test_ci_affected.py` executes both validator fixture
suites in its existing lightweight parser-ownership job. This still is NOT a
required GitHub commit status: the stable event-driven status publisher and
administrator-enabled master ruleset remain tracked in
[Issue #315](https://github.com/qigao/stun/issues/315).

## Stable event-driven acceptance status (stage M2)

`.github/workflows/required-acceptance.yml` uses only trusted default-branch
Python code to publish one stable GitHub commit-status context,
`Stun required acceptance`, against the real PR source HEAD. It responds to
`pull_request_target` (open/update/ci-full label removal or addition) and
upstream `workflow_run` requested/completed events. The latter can have an
empty `pull_requests` list; the publisher therefore resolves *currently open*
master-targeting PRs by exact source SHA from the API.

Each required native job must have actually started **no earlier than the
most recent `ci/full` label admission event** from the GitHub issue-events
API. This prevents a newly added/re-added `ci/full` label from immediately
reusing older same-SHA green jobs. Re-runs are qualified using their real
latest-attempt job start times rather than the original workflow run
creation timestamp. Missing or malformed admission timestamps fail closed.

The publisher reads PR changed files and default-branch workflow policies
to infer required host jobs; it reads live per-job execution results and
requires exact-head completed SUCCESS for every applicable job. Missing,
stale, cancelled, skipped, neutral, pending, duplicated or failed evidence,
missing `ci/full`, and unknown source roots publish **failure** rather
than a misleading success. Concurrent PR heads and source changes are
rechecked before writing. Multiple open PRs with one SHA cannot bypass
the policy via another PR's successful label. The existing M0 verifier is
still the underlying evidence checker.

This workflow neither checks out the PR head nor executes PR content,
uses a status-writing token only for trusted default-branch code, and
does not run native tests, poll, or add cross-platform build runners. The
single `workflow_run` publisher is intentionally a lightweight follow-up
to existing tests. It is **not** the same as requiring a native workflow
summary; the separate commit-status context is what an administrator
must select as the required status in the master Ruleset.

The active master ruleset requires `Stun required acceptance` from GitHub
Actions. Activation must be verified against live negative and positive PR
merge states, rather than inferred from a green status alone.
The workflow must first exist on `master` before its `workflow_run`
trigger operates; the Draft PR adding it cannot itself validate the
post-merge event. GitHub API/token errors leave the status missing or
failing, which must block once the ruleset is enabled. Fork PR status
publication is subject to the base-repository token's commit-status
access; any unsupported source SHA must fail closed, not be bypassed.

See [Issue #315](https://github.com/qigao/stun/issues/315).

## Final label admission is revalidated before status publication

The publisher may spend time retrieving a complete exact-HEAD native job
matrix while another actor removes `ci/full`. The initial PR snapshot alone
is insufficient: just before writing `Stun required acceptance`, the
publisher re-reads the live PR and rejects success if its final labels are
missing, malformed or no longer contain `ci/full`. A source SHA, PR state
or base branch change remains a separate fail-closed check. Existing
`unlabeled` event publication continues to recompute status, without
executing native tests. This reduces stale-success windows. The existing protected master
Ruleset continues to require this status; no separate App issuer is deployed.

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

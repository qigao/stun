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
only jobs whose files changed in that update run. It checks both sides of
renamed/moved files. If Git history, event metadata or the comparison is
unavailable, the check **runs**, rather than silently being skipped.

The rapid-iteration path is:

| New change | Actual heavy validation |
| --- | --- |
| Flowchart lexer/parser or golden regression | Stun runtime chart CTests and independent fast architecture check; standalone layout/graphics matrices are skipped |
| GraphLayout algorithms | GraphLayout native matrix and runtime |
| gCanvas renderer or Android platform | Relevant GLES/Android/package/cache checks and runtime |
| C11 Markdown | UI C11 released-SDK matrix and runtime |
| Controller/provider or public dependency boundary | Public provider contracts and runtime |
| Documentation only | Fast architecture check; unrelated native/SDK runs are skipped |

Matrix and sanitizer coverage has **not** been weakened, and the integration
tests and golden fixtures have not been altered by this CI refactor.

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

## Maintenance

- Add new files/paths to both the top-level trigger and
  `.github/scripts/ci-affected.sh` invocation when expanding subsystem scope.
- Keep `push` for shared path-specific workflows restricted to `master`, so
  a feature branch with an open PR does not duplicate every job.
- Keep `concurrency.cancel-in-progress` for obsolete results but never
  interpret cancellation as test success.
- Keep full runtime integration, independent published-SDK tests, sanitizers,
  Android emulator coverage and graphics pixel tests available for changes to
  their owning subsystems and for pre-merge requalification.
- Validate routing behavior with
  `python3 .github/scripts/tests/test_ci_affected.py`.

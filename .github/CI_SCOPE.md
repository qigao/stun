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
executing native tests. This reduces stale-success windows; it is not a
substitute for the dedicated App issuer tracked in
[Issue #322](https://github.com/qigao/stun/issues/322).

## Dedicated attestor webhook admission contract (Issue #322; preparatory only)

The staged `.github/scripts/attestor_webhook.py` is a **credential-free**
webhook admission layer for a *future external* dedicated GitHub App. It is
**not** a web server, an App installation, a token issuer, or a status
publisher, and is not invoked by GitHub Actions for live status issuance.
Its only use in repository Actions is an isolated, no-network fixture suite
(`test_attestor_webhook.py`) in the already-required Public Dependency job.

A trusted external service must supply the App's high-entropy webhook secret
and exact `repository.full_name`, `repository.id` and `installation.id`
from secured runtime configuration. Admission verifies HMAC-SHA256 over
the original request bytes with constant-time comparison; rejects missing
or duplicate signatures, legacy SHA1 signatures, wrong App installation,
malformed event identity and oversized payloads; and normalizes GitHub App
`pull_request` / `workflow_run` events for the existing *read-only* M0–M2
evidence evaluator. It accepts workflow events only for pinned existing
PR-test workflow paths. It does not execute PR code or trust webhook data
as native test evidence.

**Durability is an external requirement, not an implemented service:**
`enqueue(delivery_id, body_sha256, normalized_event)` must commit a
bounded, atomic delivery+payload-digest deduplication record and work item
before the webhook endpoint acknowledges receipt. It must return an
explicit boolean for newly enqueued versus already durably recorded,
and raise on persistence failures or ID/digest conflicts. Queue processing,
installation-token minting, deployment, rate limits, retries, monitoring,
and HTTPS termination are intentionally not supplied by this module.

GitHub's `X-Hub-Signature-256` authenticates the **body only**, not
`X-GitHub-Delivery`; the latter is a correlation/deduplication hint, not
cryptographic replay proof. A captured signed payload could still be
redelivered with another header. The external worker must re-fetch current
PR HEAD, labels, last `ci/full` admission, latest actual required Jobs,
and active repository ownership before deciding any commit status. Replay
cannot authorize acceptance based solely on old webhook contents.

The proposed new App-owned context is **`Stun attested acceptance`**.
The current protected `master` still requires the existing GitHub Actions
context `Stun required acceptance` (App ID 15368); this preparatory module
does **not** change that Ruleset. Before a cutover, create and install the
dedicated App, keep all private keys/tokens and webhooks outside repository
Actions, validate live positive and negative status updates, require both
old and new contexts temporarily, and only then remove the old context.
Track all missing external dependencies in
[Issue #322](https://github.com/qigao/stun/issues/322).

## External attestor bounded durable inbox (Issue #322; preparatory M1)

`.github/scripts/attestor_inbox.py` implements a **bounded SQLite WAL
inbox** which can be injected as the atomic `enqueue` callback of the
signed webhook admission module. It uses `BEGIN IMMEDIATE` transactions,
foreign keys and `synchronous=FULL`. A new delivery and its normalized
authenticated event are committed together; duplicate UUID/body digests
are not scheduled twice, and a reused UUID with a different digest is
rejected. Storage capacity is explicitly bounded (no silent eviction),
including retained terminal records, and persistence errors must prevent
acknowledging a new delivery.

The consumer enforces a **single globally claimed work item** even across
independent SQLite connections. `claim_next(owner)` records an explicit
ownership claim; only that owner can complete or fail the event. Failed
or interrupted claims are **never automatically retried/reclaimed**.
A crashed owner leaves work claimed, blocking further processing until a
trusted operator inspects the evidence and explicitly recovers the inbox.
This is intentional: a retry must not blindly republish stale-success
evidence. A new worker must always fetch the latest actual PR/CI facts
through the existing M0–M2 evaluator before any status write.

The inbox stores normalized **event metadata**, not App credentials or
private keys. An external deployment must provision a private directory,
restrict access to the SQLite database and WAL files, monitor bounded
capacity/retained records, and implement an explicit operator-controlled
recovery and retention policy. This repository does **not** implement a
public webhook listener, background worker, App installation token,
external secrets store, delivery acknowledger or new status publication.
The 15 deterministic offline inbox tests, including a SQL fault trigger
and process-reopen ownership cases, run in the existing required Public
Dependency job; no additional native CI runner is introduced.

As with the admission layer, the inbox does not turn the generic GitHub
Actions App identity into an exclusive attestor. The protected master
Ruleset continues to require `Stun required acceptance` unchanged.
Dedicated App installation, securely deployed worker, new App-owned
`Stun attested acceptance` context and the two-context Ruleset migration
remain in [Issue #322](https://github.com/qigao/stun/issues/322).

## External dedicated-App one-shot worker (Issue #322; preparatory M2)

`.github/scripts/attestor_worker.py` is an **offline-callable one-shot
worker contract**, NOT a hosted service. It consumes only envelopes that
were HMAC-verified by the admission module and durably queued in the SQLite
inbox, claims one work item under a bounded owner, and reuses the trusted
default-branch M0/M1/M2 `evaluate_event` source-HEAD/native-job verifier.

The external launcher must inject: its pinned trusted policy directory,
read-only GitHub App installation REST client, repository/installation IDs,
expected **dedicated App Bot user ID**, and an App-installation-token-backed
status writer. The worker requests only the **new**
`Stun attested acceptance` context and checks the actual API response's
SHA, state, context, Bot type and configured issuer Bot ID. A wrong issuer,
missing credential, API error or malformed response makes the work item
terminal `failed`, with **no GITHUB_TOKEN fallback** and no automatic retry.
A genuine negative CI finding (failed/skipped/pending native job) is a
*completed, correctly published failure*, not a worker error. A stale or
closed PR event is completed without publishing any status.

The worker serializes through the inbox's single-claimed-item contract.
An external service must still implement HTTPS, authenticated webhook
receiving, App private-key storage, installation-token minting/rotation,
durable delivery acknowledgements, run/process supervision, monitoring,
manual recovery and explicit deployment approval. The current repository
code neither mints App tokens nor deploys anything. The offline
`test_attestor_worker.py` fixtures verify both positive and negative
status decisions, wrong App Bot source, incorrect status context,
credential/API failures and stale events in the existing Public CI job.

Deployment must load pinned reviewed default-branch policy *from a
controlled external release*, not live PR files or an automatically
deployed, unreviewed branch. See [Issue #322](https://github.com/qigao/stun/issues/322).
Until a dedicated App is installed and its own status tested, the active
master Ruleset must continue to require the old GitHub Actions context.
No dual-context Ruleset migration is performed by this PR.

## Pin exact immutable external attestor policy releases (Issue #322, M3)

The external dedicated-App worker must not accept an arbitrary directory
containing `.github/scripts/ci_required_matrix.py` as a trusted policy
deployment. `.github/scripts/attestor_policy.py` computes a deterministic
SHA256 over the exact relative paths, length and bytes of the seven loaded
attestor Python modules and all native workflow YAML used by the trusted
M0/M1/M2 ownership matrix. Missing, empty, oversized, symlinked or modified
files are rejected; the actual **loaded Python module locations** must also
come from the same pinned root. These checks run **before claiming** any
webhook work item or publishing any commit status.

`attestor_worker.process_one(..., expected_policy_digest=...)` now REQUIRES
an explicit 64-hex digest provided by the **independent external deployment
operator**, never one automatically computed from the same runtime directory,
PR input, GitHub Actions variables or webhook body. An immutable, read-only
release mount for the lifetime of each worker process is still mandatory:
hashing alone cannot prevent a concurrent file replacement between
verification and execution. An approver must record the reviewed source
revision, independent release checksum and exact installed App identity
before enabling publication.

The Public Dependency parser job executes `test_attestor_policy.py` and
the updated `test_attestor_worker.py` offline: deterministic positive
release hash, missing/invalid digest, source/YAML modification, symlink
traversal, unexpected loaded module and pre-claim rejection. No new Actions
runner or native build is added. These are **policy release correctness
tests**, NOT App deployment or exclusive signer verification.

Current protected master Ruleset #26090409 remains Active and continues
requiring GitHub Actions `Stun required acceptance`. The dedicated App
identity, trusted external policy deployment, installation tokens, actual
HTTPS ingress and `Stun attested acceptance` dual-status migration remain
open in [Issue #322](https://github.com/qigao/stun/issues/322).

## Explicit operator quarantine of orphaned attestor claims (Issue #322, M4)

`.github/scripts/attestor_inbox.py` retains its bounded single-owner
SQLite WAL work queue and deliberate **no automatic retry** policy. After a
process crash leaves a record in `claimed`, the only supported change is
a separately authorized operator's **irreversible terminal failure**:
`inbox.quarantine_claim(digest, claimed_owner, operator=..., reason=...,
evidence_ref=...)`. It uses the same `BEGIN IMMEDIATE` transaction to
atomically verify that the exact claim still belongs to that owner, mark
it `failed` and append a `operator_quarantine` audit entry. Wrong owner,
repeat action, missing operator/evidence or a completed/failed claim are
rejected. The audit table refuses UPDATE/DELETE in ordinary SQLite DML,
and an injected audit INSERT fault rolls back the entire quarantine,
preserving the old claim. Existing version-1 inboxes receive only an
additive version-2 schema upgrade; unknown versions still fail closed.

**This API is not an authentication mechanism.** The external attestor
service must restrict database access, authenticate a distinct operator,
and require an incident reference after checking the live GitHub PR,
current exact HEAD, last `ci/full` admission and any status that might
already have been posted before a crash. This operation never requeues or
replays the old webhook. Following quarantine, a different already-pending
event may be processed after normal live revalidation. Recovery from
`failed` to `pending`, automatic leases, automatic retry, status
republication and silent deletion are intentionally unsupported.

The eight additional deterministic SQLite tests run inside the already
required Public Dependency parser job, including two-connection
arbitration, reopen persistence, duplicate quarantine rejection and
audit-write failure rollback. No extra native runner, worker daemon,
private key, App identity or external deployment is created. This
operator path must remain separate from public HTTP ingress.

The active master Ruleset continues to require the old
`Stun required acceptance` context from shared GitHub Actions; exclusive
dedicated-App issuance and live dual-context migration remain Open in
[Issue #322](https://github.com/qigao/stun/issues/322).

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

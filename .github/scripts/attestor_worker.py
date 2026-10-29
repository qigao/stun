#!/usr/bin/env python3
"""One-shot, single-owner EXTERNAL GitHub App attestation worker contract.

Not a daemon, a deployed service, a token minter or an Actions job. The
externally controlled launcher must provide a pinned trusted policy tree,
read-only authenticated GitHub API client, an App-installation-only status
writer, and a protected durable inbox. No GITHUB_TOKEN or issuer fallback.
"""
from pathlib import Path

from attestor_webhook import EVENT_ACTIONS, TRUSTED_WORKFLOW_PATHS
from publish_required_acceptance import evaluate_event

ATTESTED_CONTEXT = "Stun attested acceptance"


class AttestorWorkerError(ValueError):
    """Invalid identity or status issuer; never quietly accept or retry."""


def _positive_int(value):
    return isinstance(value, int) and not isinstance(value, bool) and value > 0


def _validate_record(event, repo, repository_id, installation_id):
    if not isinstance(event, dict):
        raise AttestorWorkerError("inbox event is not an object")
    source = event.get("repository")
    installation = event.get("installation")
    if (not isinstance(source, dict) or not isinstance(installation, dict)
            or source.get("full_name") != repo
            or source.get("id") != repository_id
            or installation.get("id") != installation_id):
        raise AttestorWorkerError("inbox record mismatches dedicated installation")

    name, action = event.get("_event_name"), event.get("action")
    if name == "pull_request_target":
        kind = "pull_request"
        pr = event.get("pull_request")
        if (not isinstance(pr, dict) or not isinstance(pr.get("base"), dict)
                or pr["base"].get("ref") != "master"):
            raise AttestorWorkerError("PR target was changed in durable record")
        base_repo = pr["base"].get("repo")
        if not isinstance(base_repo, dict) or base_repo.get("full_name") != repo:
            raise AttestorWorkerError("PR base repository is not trusted")
        if action in ("labeled", "unlabeled"):
            label = event.get("label")
            if not isinstance(label, dict) or label.get("name") != "ci/full":
                raise AttestorWorkerError("irrelevant label cannot attest")
    elif name == "workflow_run":
        kind = name
        run = event.get("workflow_run")
        if (not isinstance(run, dict) or run.get("event") != "pull_request"
                or run.get("path") not in TRUSTED_WORKFLOW_PATHS):
            raise AttestorWorkerError("untrusted workflow event")
    else:
        raise AttestorWorkerError("unsupported queued event kind")
    if action not in EVENT_ACTIONS[kind]:
        raise AttestorWorkerError("invalid queued event action")


def process_one(inbox, owner, *, repository, repository_id, installation_id,
                issuer_bot_id, trusted_policy_root, github_get,
                write_app_status):
    """Claim at most one queued event, verify live CI, publish using App writer.

    write_app_status(sha, state, description, pr_number, context) must use
    ONLY an externally minted dedicated GitHub App INSTALLATION token. The
    callback returns GitHub's actual status object including creator.id,
    state, sha and context. This function checks them; it NEVER sources a
    token from environment or falls back to GitHub Actions credentials.

    A genuine negative CI decision is a completed work item (publishes
    failure). Token/API/identity failures are terminal failed items, with
    no automatic retries. A stale event for a closed/rebased PR is completed
    without status, because evaluate_event re-fetches current PR identity.
    """
    if (not isinstance(repository, str) or repository != "qigao/stun"
            or not all(_positive_int(x) for x in (
                repository_id, installation_id, issuer_bot_id
            )) or not callable(github_get) or not callable(write_app_status)):
        raise AttestorWorkerError("missing explicit dedicated App identity or API")
    root = Path(trusted_policy_root)
    if (not root.is_absolute() or
            not (root / ".github/scripts/ci_required_matrix.py").is_file() or
            not (root / ".github/workflows").is_dir()):
        raise AttestorWorkerError("missing pinned trusted policy installation")

    item = inbox.claim_next(owner)
    if item is None:
        return None
    try:
        _validate_record(item.event, repository, repository_id, installation_id)

        def verified_status(sha, state, description, number):
            response = write_app_status(
                sha, state, description, number, ATTESTED_CONTEXT
            )
            if not isinstance(response, dict):
                raise AttestorWorkerError("dedicated App status response missing")
            creator = response.get("creator")
            if (response.get("context") != ATTESTED_CONTEXT or
                    response.get("sha") != sha or
                    response.get("state") != state or
                    not isinstance(creator, dict) or
                    creator.get("id") != issuer_bot_id or
                    creator.get("type") != "Bot"):
                raise AttestorWorkerError(
                    "status was not confirmed from dedicated App Bot identity"
                )

        outcomes = evaluate_event(
            item.event, github_get, verified_status, repository, root
        )
        # A published SUCCESS is allowed only by trusted M0–M2 exact-head
        # evidence checks; a published FAILURE is also valid work completion.
        inbox.complete(item.digest, owner, successful=True)
        return outcomes
    except Exception:
        # Explicit terminal state, never an unbounded or automatic retry.
        inbox.complete(item.digest, owner, successful=False)
        raise

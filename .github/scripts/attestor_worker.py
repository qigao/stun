#!/usr/bin/env python3
"""One-shot, single-owner EXTERNAL GitHub App attestation worker contract.

Not a daemon, a deployed service, a token minter or an Actions job. The
externally controlled launcher must provide an immutable policy tree
and an independently approved SHA256 digest, a read-only GitHub API client,
an App-installation-only status writer, and a protected durable inbox.
No GITHUB_TOKEN, self-generated digest or status-issuer fallback.
"""
from pathlib import Path

import attestor_webhook
import attestor_inbox
import attestor_policy
import ci_required_matrix
import publish_required_acceptance
import verify_exact_head_ci
from attestor_policy import verify_policy_bundle
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
                issuer_bot_id, trusted_policy_root, expected_policy_digest,
                github_get, write_app_status):
    """Claim at most one queued event, verify live CI, publish using App writer.

    write_app_status(sha, state, description, pr_number, context) must use
    ONLY an externally minted dedicated GitHub App INSTALLATION token. The
    callback returns GitHub's actual status object including creator.id,
    state, sha and context. This function checks them; it NEVER sources a
    token from environment or falls back to GitHub Actions credentials.

    expected_policy_digest is pinned outside the repository/Actions/webhook.
    The deployed policy mount must remain immutable for the worker lifetime:
    SHA verification cannot protect a mutable filesystem against TOCTOU.

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
    # The expected hash comes from an independently approved external release
    # configuration. Never compute it from untrusted files and accept that
    # value as the expected digest. Both loaded modules and on-disk policy
    # are verified before any ownership claim or status publication.
    root = Path(trusted_policy_root)
    if type(inbox) is not attestor_inbox.DurableWebhookInbox:
        raise AttestorWorkerError("untrusted inbox implementation")
    loaded = {
        ".github/scripts/attestor_policy.py": attestor_policy.__file__,
        ".github/scripts/attestor_worker.py": __file__,
        ".github/scripts/attestor_webhook.py": attestor_webhook.__file__,
        ".github/scripts/attestor_inbox.py": attestor_inbox.__file__,
        ".github/scripts/ci_required_matrix.py": ci_required_matrix.__file__,
        ".github/scripts/verify_exact_head_ci.py": verify_exact_head_ci.__file__,
        ".github/scripts/publish_required_acceptance.py":
            publish_required_acceptance.__file__,
    }
    verify_policy_bundle(root, expected_policy_digest,
                         loaded_module_paths=loaded)

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

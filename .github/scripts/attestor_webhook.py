#!/usr/bin/env python3
"""Signed webhook admission contract for an EXTERNAL dedicated GitHub App.

This module deliberately has no HTTP server, credential store, GitHub App
private key, installation-token minting or status-write capability. A trusted
external service must provide a durable atomic queue and an App-owned worker.
It MUST NOT run from a pull-request checkout or a repository Actions runner.
"""
from dataclasses import dataclass
import hashlib
import hmac
import json
import re
from uuid import UUID

from ci_required_matrix import WORKFLOWS, PUBLIC, RUNTIME

MAX_WEBHOOK_BYTES = 1024 * 1024
EVENT_ACTIONS = {
    "pull_request": frozenset((
        "opened", "synchronize", "reopened", "edited", "labeled", "unlabeled"
    )),
    "workflow_run": frozenset(("requested", "completed")),
}
TRUSTED_WORKFLOW_PATHS = frozenset(
    {PUBLIC, RUNTIME} | {".github/workflows/" + n for n in WORKFLOWS}
)
SHA = re.compile(r"[0-9a-f]{40}\Z")
SIGNATURE = re.compile(r"sha256=[0-9a-f]{64}\Z")


class AdmissionError(ValueError):
    """Fail closed; do not enqueue, acknowledge or publish any status."""


@dataclass(frozen=True)
class AdmittedWebhook:
    delivery_id: str
    body_sha256: str
    repository: str
    installation_id: int
    event_name: str
    source_sha: str
    queued: bool
    # For the trusted worker only; never treat its content as authority.
    normalized_event: dict


def _required_header(headers, name):
    values = [v for k, v in headers.items()
              if isinstance(k, str) and k.lower() == name.lower()]
    if len(values) != 1 or not isinstance(values[0], str) or not values[0]:
        raise AdmissionError("missing, duplicated or malformed header " + name)
    return values[0]


def _positive_int(value):
    return isinstance(value, int) and not isinstance(value, bool) and value > 0


def admit_signed_delivery(headers, body, webhook_secret, *, expected_repo,
                          expected_repo_id, expected_installation_id, enqueue):
    """Admit once; enqueue must ATOMICALLY persist delivery + body digest.

    enqueue(delivery_uuid, body_sha256, normalized_event) returns True only
    when durably queued, False only for a previously persisted duplicate with
    the identical body. On persistence failure/conflict it MUST raise.
    A verified signature proves payload integrity, NOT freshness; the worker
    must always recompute the current PR/CI state before status publication.
    """
    if not isinstance(headers, dict) or not isinstance(body, bytes):
        raise AdmissionError("invalid HTTP envelope")
    if not isinstance(webhook_secret, bytes) or len(webhook_secret) < 32:
        raise AdmissionError("missing high-entropy external webhook secret")
    if not body or len(body) > MAX_WEBHOOK_BYTES:
        raise AdmissionError("webhook payload empty or exceeds size limit")
    if (not isinstance(expected_repo, str) or not expected_repo or
            not _positive_int(expected_repo_id) or
            not _positive_int(expected_installation_id) or
            not callable(enqueue)):
        raise AdmissionError("missing trusted installation configuration")

    signature = _required_header(headers, "X-Hub-Signature-256")
    if SIGNATURE.fullmatch(signature) is None:
        raise AdmissionError("missing valid SHA256 signature")
    computed = "sha256=" + hmac.new(
        webhook_secret, body, hashlib.sha256
    ).hexdigest()
    if not hmac.compare_digest(computed, signature):
        raise AdmissionError("webhook signature mismatch")

    event_name = _required_header(headers, "X-GitHub-Event")
    if event_name not in EVENT_ACTIONS:
        raise AdmissionError("unsubscribed GitHub event")
    delivery_id = _required_header(headers, "X-GitHub-Delivery")
    try:
        parsed_uuid = UUID(delivery_id)
    except (ValueError, AttributeError) as exc:
        raise AdmissionError("invalid delivery correlation id") from exc
    if parsed_uuid.version not in (1, 4):
        raise AdmissionError("unexpected delivery UUID type")

    # Verify bytes BEFORE decoding, and never mutate the raw signed payload.
    try:
        event = json.loads(body.decode("utf-8"))
    except (UnicodeError, ValueError) as exc:
        raise AdmissionError("malformed UTF-8 JSON webhook") from exc
    if not isinstance(event, dict):
        raise AdmissionError("event payload must be an object")
    repository, installation = event.get("repository"), event.get("installation")
    if not isinstance(repository, dict) or not isinstance(installation, dict):
        raise AdmissionError("missing repository/installation identity")
    if (repository.get("full_name") != expected_repo or
            repository.get("id") != expected_repo_id or
            installation.get("id") != expected_installation_id):
        raise AdmissionError("webhook repository/installation mismatch")
    if event.get("action") not in EVENT_ACTIONS[event_name]:
        raise AdmissionError("unrecognized event action")

    if event_name == "pull_request":
        pr = event.get("pull_request")
        if not isinstance(pr, dict) or not _positive_int(pr.get("number")):
            raise AdmissionError("missing PR identity")
        base = pr.get("base")
        if (not isinstance(base, dict) or base.get("ref") != "master" or
                not isinstance(base.get("repo"), dict) or
                base["repo"].get("full_name") != expected_repo):
            raise AdmissionError("PR does not target trusted master")
        sha = pr.get("head", {}).get("sha") if isinstance(pr.get("head"), dict) else None
        name_for_worker = "pull_request_target"
        if event.get("action") in ("labeled", "unlabeled"):
            label = event.get("label")
            if not isinstance(label, dict) or label.get("name") != "ci/full":
                raise AdmissionError("unrelated PR label")
    else:
        run = event.get("workflow_run")
        if not isinstance(run, dict):
            raise AdmissionError("missing workflow_run")
        if (run.get("event") != "pull_request" or
                run.get("path") not in TRUSTED_WORKFLOW_PATHS):
            raise AdmissionError("workflow is not a trusted PR validation")
        sha = run.get("head_sha")
        name_for_worker = "workflow_run"
    if not isinstance(sha, str) or SHA.fullmatch(sha) is None:
        raise AdmissionError("invalid source SHA")

    normalized = dict(event)
    # GitHub App sends pull_request, while the legacy trusted evaluator uses
    # the Actions pull_request_target event name. It still RE-FETCHES all PRs.
    normalized["_event_name"] = name_for_worker
    digest = hashlib.sha256(body).hexdigest()
    queued = enqueue(str(parsed_uuid), digest, normalized)
    if type(queued) is not bool:
        raise AdmissionError("durable enqueue must return explicit boolean")
    return AdmittedWebhook(
        delivery_id=str(parsed_uuid), body_sha256=digest,
        repository=expected_repo, installation_id=expected_installation_id,
        event_name=event_name, source_sha=sha,
        queued=queued, normalized_event=normalized
    )

#!/usr/bin/env python3
"""No-network webhook authentication tests for external App admission."""
import copy
import hashlib
import hmac
import json
from pathlib import Path
import sys
import unittest
from uuid import UUID

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from attestor_webhook import (  # noqa: E402
    AdmissionError, MAX_WEBHOOK_BYTES, admit_signed_delivery,
)

REPO = "qigao/stun"
REPO_ID = 1070504466
INSTALLATION_ID = 246800
WEBHOOK_SECRET = b"unit-tests-only-not-a-production-webhook-secret-2026"
FIRST_DELIVERY = "198e90bc-c7a1-44cf-bce1-c1722c80fb83"
SECOND_DELIVERY = "41a96ce2-3149-4a36-977f-e1acace79ef9"
HEAD = "a" * 40


def pr_event(action="opened"):
    return {
        "action": action, "repository": {"id": REPO_ID, "full_name": REPO},
        "installation": {"id": INSTALLATION_ID},
        "number": 9,
        "pull_request": {
            "number": 9, "state": "open",
            "head": {"sha": HEAD},
            "base": {"ref": "master", "repo": {"full_name": REPO}},
        },
    }


def run_event(action="completed"):
    return {
        "action": action, "repository": {"id": REPO_ID, "full_name": REPO},
        "installation": {"id": INSTALLATION_ID},
        "workflow_run": {
            "event": "pull_request", "head_sha": HEAD,
            "path": ".github/workflows/runtime-conformance.yml",
            "pull_requests": [],
        },
    }


def encode(event, kind="pull_request", delivery=FIRST_DELIVERY):
    body = json.dumps(event, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    signature = "sha256=" + hmac.new(
        WEBHOOK_SECRET, body, hashlib.sha256
    ).hexdigest()
    return {
        "X-Hub-Signature-256": signature,
        "X-GitHub-Event": kind,
        "X-GitHub-Delivery": delivery,
    }, body


class FakeDurableQueue:
    """Fixture only: real production queue MUST be durable and atomic."""

    def __init__(self):
        self.by_delivery = {}
        self.by_digest = {}
        self.payloads = []

    def enqueue(self, delivery, digest, payload):
        if delivery in self.by_delivery:
            if self.by_delivery[delivery] != digest:
                raise AdmissionError("collision in delivery id")
            return False
        if digest in self.by_digest:
            self.by_delivery[delivery] = digest
            return False
        self.by_delivery[delivery] = digest
        self.by_digest[digest] = delivery
        self.payloads.append(copy.deepcopy(payload))
        return True


def admit(event=None, kind="pull_request", queue=None, delivery=FIRST_DELIVERY):
    queue = queue or FakeDurableQueue()
    headers, body = encode(event or pr_event(), kind, delivery)
    outcome = admit_signed_delivery(
        headers, body, WEBHOOK_SECRET,
        expected_repo=REPO, expected_repo_id=REPO_ID,
        expected_installation_id=INSTALLATION_ID,
        enqueue=queue.enqueue
    )
    return outcome, queue


class AuthenticatedAdmissionTests(unittest.TestCase):
    def assert_reject(self, event, *, kind="pull_request", mutate=None):
        queue = FakeDurableQueue()
        headers, body = encode(event, kind)
        if mutate:
            headers, body = mutate(headers, body)
        with self.assertRaises(AdmissionError):
            admit_signed_delivery(
                headers, body, WEBHOOK_SECRET,
                expected_repo=REPO, expected_repo_id=REPO_ID,
                expected_installation_id=INSTALLATION_ID,
                enqueue=queue.enqueue
            )
        self.assertEqual(queue.payloads, [])

    def test_pr_identity_preserved_without_executing_it(self):
        item, queue = admit()
        self.assertTrue(item.queued)
        self.assertEqual(item.repository, REPO)
        self.assertEqual(item.installation_id, INSTALLATION_ID)
        self.assertEqual(item.source_sha, HEAD)
        self.assertEqual(item.normalized_event["_event_name"], "pull_request_target")
        self.assertEqual(len(queue.payloads), 1)

    def test_real_workflow_run_normalizes_to_trusted_evaluator(self):
        item, queue = admit(run_event(), "workflow_run")
        self.assertTrue(item.queued)
        self.assertEqual(item.normalized_event["_event_name"], "workflow_run")
        self.assertEqual(item.normalized_event["workflow_run"]["pull_requests"], [])
        self.assertEqual(len(queue.payloads), 1)

    def test_github_documented_signature_reference(self):
        key = b"It's a Secret to Everybody"
        digest = hmac.new(key, b"Hello, World!", hashlib.sha256).hexdigest()
        self.assertEqual(
            digest, "757107ea0eb2509fc211221cce984b8a37570b6d7586c22c46f4379c8b043e17"
        )

    def test_reject_tampered_body_and_signature(self):
        self.assert_reject(pr_event(), mutate=lambda h, b: (h, b + b" "))
        self.assert_reject(pr_event(), mutate=lambda h, b: (
            {**h, "X-Hub-Signature-256": "sha256=" + "0" * 64}, b
        ))

    def test_reject_sha1_and_missing_or_duplicate_signature(self):
        for mutate in (
            lambda h, b: ({k: v for k, v in h.items()
                           if k != "X-Hub-Signature-256"}, b),
            lambda h, b: ({**h, "X-Hub-Signature-256": "sha1=" + "0" * 40}, b),
            lambda h, b: ({**h, "x-hub-signature-256": h["X-Hub-Signature-256"]}, b),
        ):
            self.assert_reject(pr_event(), mutate=mutate)

    def test_reject_wrong_repo_id_name_or_installation(self):
        for key, value in (
            ("id", REPO_ID + 1), ("full_name", "other/stun"),
        ):
            event = pr_event()
            event["repository"][key] = value
            self.assert_reject(event)
        event = pr_event()
        event["installation"]["id"] = INSTALLATION_ID + 1
        self.assert_reject(event)

    def test_reject_fork_head_targeting_other_base(self):
        for field, value in (("ref", "develop"), ("repo", {"full_name": "other/stun"})):
            event = pr_event()
            event["pull_request"]["base"][field] = value
            self.assert_reject(event)

    def test_reject_untrusted_workflow_path_or_non_pr_run(self):
        for key, value in (
            ("path", ".github/workflows/required-acceptance.yml"),
            ("path", ".github/workflows/unknown.yml"),
            ("event", "push"),
        ):
            event = run_event()
            event["workflow_run"][key] = value
            self.assert_reject(event, kind="workflow_run")

    def test_reject_unrelated_label(self):
        event = pr_event("labeled")
        event["label"] = {"name": "documentation"}
        self.assert_reject(event)
        event["label"]["name"] = "ci/full"
        item, queue = admit(event)
        self.assertTrue(item.queued)
        self.assertEqual(len(queue.payloads), 1)

    def test_reject_unknown_event_action_or_source_sha(self):
        self.assert_reject(pr_event("closed"))
        event = pr_event()
        event["pull_request"]["head"]["sha"] = "not-a-sha"
        self.assert_reject(event)
        event = run_event()
        event["workflow_run"]["head_sha"] = "f" * 39
        self.assert_reject(event, kind="workflow_run")

    def test_reject_unknown_webhook_event_and_invalid_uuid(self):
        self.assert_reject(pr_event(), mutate=lambda h, b: (
            {**h, "X-GitHub-Event": "push"}, b
        ))
        self.assert_reject(pr_event(), mutate=lambda h, b: (
            {**h, "X-GitHub-Delivery": "not-an-uuid"}, b
        ))

    def test_reject_empty_oversize_or_invalid_json(self):
        self.assert_reject(pr_event(), mutate=lambda h, b: (h, b""))
        self.assert_reject(pr_event(), mutate=lambda h, b: (
            h, b"x" * (MAX_WEBHOOK_BYTES + 1)
        ))
        self.assert_reject(pr_event(), mutate=lambda h, b: (h, b"\xff"))

    def test_valid_signature_does_not_authorize_invalid_json(self):
        malformed = b'{"repository":'
        headers, _ = encode(pr_event())
        headers["X-Hub-Signature-256"] = "sha256=" + hmac.new(
            WEBHOOK_SECRET, malformed, hashlib.sha256
        ).hexdigest()
        queue = FakeDurableQueue()
        with self.assertRaises(AdmissionError):
            admit_signed_delivery(
                headers, malformed, WEBHOOK_SECRET,
                expected_repo=REPO, expected_repo_id=REPO_ID,
                expected_installation_id=INSTALLATION_ID,
                enqueue=queue.enqueue
            )
        self.assertEqual(queue.payloads, [])

    def test_signed_utf8_payload_preserves_unicode(self):
        event = pr_event()
        event["summary"] = "CJK 汉字と文字"
        item, queue = admit(event)
        self.assertTrue(item.queued)
        self.assertEqual(queue.payloads[0]["summary"], "CJK 汉字と文字")

    def test_invalid_external_secret_cannot_admit(self):
        queue = FakeDurableQueue()
        headers, body = encode(pr_event())
        with self.assertRaises(AdmissionError):
            admit_signed_delivery(
                headers, body, b"short",
                expected_repo=REPO, expected_repo_id=REPO_ID,
                expected_installation_id=INSTALLATION_ID,
                enqueue=queue.enqueue
            )
        self.assertFalse(queue.payloads)

    def test_redelivery_and_resigned_delivery_are_idempotent(self):
        queue = FakeDurableQueue()
        first, queue = admit(queue=queue)
        repeat, queue = admit(queue=queue)
        fresh_delivery, queue = admit(queue=queue, delivery=SECOND_DELIVERY)
        self.assertTrue(first.queued)
        self.assertFalse(repeat.queued)
        self.assertFalse(fresh_delivery.queued)
        self.assertEqual(len(queue.payloads), 1)
        self.assertEqual(first.body_sha256, repeat.body_sha256)
        self.assertEqual(first.body_sha256, fresh_delivery.body_sha256)

    def test_delivery_collision_fails_closed(self):
        queue = FakeDurableQueue()
        admit(queue=queue)
        event = pr_event()
        event["pull_request"]["head"]["sha"] = "b" * 40
        headers, body = encode(event)
        with self.assertRaises(AdmissionError):
            admit_signed_delivery(
                headers, body, WEBHOOK_SECRET,
                expected_repo=REPO, expected_repo_id=REPO_ID,
                expected_installation_id=INSTALLATION_ID,
                enqueue=queue.enqueue
            )
        self.assertEqual(len(queue.payloads), 1)

    def test_durable_storage_failure_prevents_acknowledgement(self):
        headers, body = encode(pr_event())
        def enqueue(*_):
            raise OSError("database not committed")
        with self.assertRaises(OSError):
            admit_signed_delivery(
                headers, body, WEBHOOK_SECRET,
                expected_repo=REPO, expected_repo_id=REPO_ID,
                expected_installation_id=INSTALLATION_ID,
                enqueue=enqueue
            )

    def test_explicit_enqueue_result_only(self):
        headers, body = encode(pr_event())
        with self.assertRaises(AdmissionError):
            admit_signed_delivery(
                headers, body, WEBHOOK_SECRET,
                expected_repo=REPO, expected_repo_id=REPO_ID,
                expected_installation_id=INSTALLATION_ID,
                enqueue=lambda *_: "success"
            )

    def test_secret_key_repo_id_and_installation_are_runtime_inputs(self):
        headers, body = encode(pr_event())
        for kwargs in (
            {"expected_repo": ""},
            {"expected_repo_id": True},
            {"expected_installation_id": None},
        ):
            args = dict(
                expected_repo=REPO, expected_repo_id=REPO_ID,
                expected_installation_id=INSTALLATION_ID,
                enqueue=FakeDurableQueue().enqueue,
            )
            args.update(kwargs)
            with self.assertRaises(AdmissionError):
                admit_signed_delivery(headers, body, WEBHOOK_SECRET, **args)


if __name__ == "__main__":
    unittest.main(verbosity=1)

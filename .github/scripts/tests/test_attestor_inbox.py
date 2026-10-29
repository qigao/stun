#!/usr/bin/env python3
"""Deterministic fault/ownership regressions for the external App inbox."""
import hashlib
import hmac
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest
from uuid import uuid4

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from attestor_inbox import DurableWebhookInbox, InboxError, MAX_EVENT_BYTES
from attestor_webhook import admit_signed_delivery

SHA = "a" * 40
EVENT = {
    "_event_name": "pull_request_target",
    "action": "opened", "pull_request": {"head": {"sha": SHA}},
}
D1 = "cc576825-3aab-43b8-96be-4a1c7230aaf1"
D2 = "30f1a8bc-c8f5-493b-9b54-bae0d013c1d4"
D3 = "e94cd459-a374-4651-9d22-fab8121509ce"


def digest(payload=EVENT):
    body = json.dumps(payload, ensure_ascii=False, sort_keys=True,
                      separators=(",", ":"), allow_nan=False).encode("utf-8")
    return hashlib.sha256(body).hexdigest()


class InboxTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="stun-attestor-inbox-")
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name) / "events.sqlite3"
        self.inbox = DurableWebhookInbox(self.path, max_events=3, max_deliveries=5)
        self.addCleanup(self.inbox.close)

    def test_atomic_enqueue_persists_across_reopen(self):
        self.assertTrue(self.inbox.enqueue(D1, digest(), EVENT))
        self.assertEqual(self.inbox.summary()["pending"], 1)
        other = DurableWebhookInbox(self.path, max_events=3, max_deliveries=5)
        try:
            self.assertEqual(other.summary()["pending"], 1)
            job = other.claim_next("worker-A")
            self.assertEqual(job.digest, digest())
            self.assertEqual(job.event, EVENT)
            self.assertEqual(job.owner, "worker-A")
            other.complete(job.digest, job.owner, successful=True)
            self.assertEqual(other.summary()["completed"], 1)
        finally:
            other.close()
        self.assertEqual(self.inbox.summary()["completed"], 1)

    def test_duplicate_uuid_and_duplicate_body_cannot_double_enqueue(self):
        self.assertTrue(self.inbox.enqueue(D1, digest(), EVENT))
        self.assertFalse(self.inbox.enqueue(D1, digest(), EVENT))
        self.assertFalse(self.inbox.enqueue(D2, digest(), EVENT))
        self.assertEqual(self.inbox.summary(), {
            "pending": 1, "claimed": 0, "completed": 0,
            "failed": 0, "deliveries": 2,
        })

    def test_same_uuid_with_different_body_is_error(self):
        self.inbox.enqueue(D1, digest(), EVENT)
        changed = {**EVENT, "source": "another"}
        with self.assertRaises(InboxError):
            self.inbox.enqueue(D1, digest(changed), changed)
        self.assertEqual(self.inbox.summary()["deliveries"], 1)
        self.assertEqual(self.inbox.summary()["pending"], 1)

    def test_only_one_claimed_owner_across_two_connections(self):
        other_event = {**EVENT, "source": "other"}
        self.inbox.enqueue(D1, digest(), EVENT)
        self.inbox.enqueue(D2, digest(other_event), other_event)
        other = DurableWebhookInbox(self.path, max_events=3, max_deliveries=5)
        try:
            first = self.inbox.claim_next("worker-A")
            self.assertEqual(first.digest, digest())
            self.assertIsNone(other.claim_next("worker-B"))
            self.inbox.complete(first.digest, "worker-A", successful=True)
            second = other.claim_next("worker-B")
            self.assertEqual(second.digest, digest(other_event))
            self.assertEqual(other.summary()["claimed"], 1)
        finally:
            other.close()

    def test_crash_claim_does_not_auto_retry(self):
        other_event = {**EVENT, "kind": "second"}
        self.inbox.enqueue(D1, digest(), EVENT)
        self.inbox.enqueue(D2, digest(other_event), other_event)
        self.inbox.claim_next("crashed-owner")
        other = DurableWebhookInbox(self.path, max_events=3, max_deliveries=5)
        try:
            self.assertIsNone(other.claim_next("replacement-worker"))
            self.assertEqual(other.summary()["claimed"], 1)
            self.assertEqual(other.summary()["pending"], 1)
        finally:
            other.close()

    def test_failed_claim_is_terminal_without_implicit_retry(self):
        later = {**EVENT, "later": 1}
        self.inbox.enqueue(D1, digest(), EVENT)
        self.inbox.enqueue(D2, digest(later), later)
        item = self.inbox.claim_next("owner-1")
        self.inbox.complete(item.digest, "owner-1", successful=False)
        self.assertEqual(self.inbox.summary()["failed"], 1)
        selected = self.inbox.claim_next("owner-2")
        self.assertEqual(selected.digest, digest(later))
        self.assertNotEqual(selected.digest, item.digest)

    def test_wrong_owner_and_double_complete_rejected(self):
        self.inbox.enqueue(D1, digest(), EVENT)
        item = self.inbox.claim_next("owner-1")
        with self.assertRaises(InboxError):
            self.inbox.complete(item.digest, "owner-2", successful=True)
        self.assertEqual(self.inbox.summary()["claimed"], 1)
        self.inbox.complete(item.digest, "owner-1", successful=True)
        with self.assertRaises(InboxError):
            self.inbox.complete(item.digest, "owner-1", successful=True)

    def test_event_quota_includes_completed_records(self):
        self.inbox.max_events = 1
        self.inbox.enqueue(D1, digest(), EVENT)
        item = self.inbox.claim_next("owner")
        self.inbox.complete(item.digest, "owner", successful=True)
        changed = {**EVENT, "type": "new"}
        with self.assertRaises(InboxError):
            self.inbox.enqueue(D2, digest(changed), changed)
        self.assertEqual(self.inbox.summary()["completed"], 1)
        self.assertEqual(self.inbox.summary()["deliveries"], 1)

    def test_delivery_quota_blocks_unpersisted_duplicate(self):
        self.inbox.max_deliveries = 1
        self.inbox.enqueue(D1, digest(), EVENT)
        with self.assertRaises(InboxError):
            self.inbox.enqueue(D2, digest(), EVENT)
        self.assertFalse(self.inbox.enqueue(D1, digest(), EVENT))
        self.assertEqual(self.inbox.summary()["deliveries"], 1)

    def test_atomic_rollback_on_delivery_insert_fault(self):
        self.inbox.conn.execute("""
            CREATE TRIGGER inject_delivery_failure BEFORE INSERT ON deliveries
            BEGIN SELECT RAISE(ABORT,'fault injection'); END
        """)
        with self.assertRaises(sqlite3.IntegrityError):
            self.inbox.enqueue(D1, digest(), EVENT)
        self.assertEqual(self.inbox.summary()["pending"], 0)
        self.assertEqual(self.inbox.summary()["deliveries"], 0)
        self.inbox.conn.execute("DROP TRIGGER inject_delivery_failure")
        self.assertTrue(self.inbox.enqueue(D1, digest(), EVENT))

    def test_invalid_digest_uuid_owner_and_terminal_result(self):
        with self.assertRaises(InboxError):
            self.inbox.enqueue("not-uuid", digest(), EVENT)
        with self.assertRaises(InboxError):
            self.inbox.enqueue(D1, "not-digest", EVENT)
        with self.assertRaises(InboxError):
            self.inbox.enqueue(D1, digest(), ["not-an-event"])
        self.assertTrue(self.inbox.enqueue(D1, digest(), EVENT))
        with self.assertRaises(InboxError):
            self.inbox.claim_next("")
        work = self.inbox.claim_next("owner")
        with self.assertRaises(InboxError):
            self.inbox.complete(work.digest, "owner", successful="yes")
        self.assertEqual(self.inbox.summary()["claimed"], 1)

    def test_oversized_and_nonserializable_envelopes_rejected(self):
        bad_event = {"oversize": "x" * (MAX_EVENT_BYTES + 1)}
        with self.assertRaises(InboxError):
            self.inbox.enqueue(D1, digest(), bad_event)
        with self.assertRaises(InboxError):
            self.inbox.enqueue(D1, digest(), {"object": object()})
        self.assertEqual(self.inbox.summary()["pending"], 0)

    def test_db_path_must_be_trusted_absolute_file(self):
        with self.assertRaises(InboxError):
            DurableWebhookInbox("relative.sqlite3")
        link = Path(self.tmp.name) / "link.sqlite3"
        link.symlink_to(self.path)
        with self.assertRaises(InboxError):
            DurableWebhookInbox(link)

    def test_unsupported_schema_is_rejected(self):
        self.inbox.conn.execute("PRAGMA user_version=99")
        with self.assertRaises(InboxError):
            DurableWebhookInbox(self.path)
        self.inbox.conn.execute("PRAGMA user_version=1")

    def test_authenticated_webhook_contract_integration(self):
        # This public fixture secret is NEVER used for any real GitHub App.
        secret = b"dummy-only-32-character-high-entropy-secret"
        event = {
            "action": "opened",
            "repository": {"id": 1070504466, "full_name": "qigao/stun"},
            "installation": {"id": 246800},
            "number": 9,
            "pull_request": {
                "number": 9, "head": {"sha": SHA},
                "base": {"ref": "master",
                         "repo": {"full_name": "qigao/stun"}},
            },
        }
        body = json.dumps(event, separators=(",", ":")).encode()
        headers = {
            "X-Hub-Signature-256": "sha256=" + hmac.new(
                secret, body, hashlib.sha256
            ).hexdigest(),
            "X-GitHub-Event": "pull_request",
            "X-GitHub-Delivery": D3,
        }
        result = admit_signed_delivery(
            headers, body, secret, expected_repo="qigao/stun",
            expected_repo_id=1070504466,
            expected_installation_id=246800, enqueue=self.inbox.enqueue
        )
        self.assertTrue(result.queued)
        self.assertFalse(admit_signed_delivery(
            headers, body, secret, expected_repo="qigao/stun",
            expected_repo_id=1070504466,
            expected_installation_id=246800, enqueue=self.inbox.enqueue
        ).queued)
        job = self.inbox.claim_next("owner")
        self.assertEqual(job.event["_event_name"], "pull_request_target")
        self.assertEqual(job.event["pull_request"]["head"]["sha"], SHA)
        self.inbox.complete(job.digest, job.owner, successful=True)
        self.assertEqual(self.inbox.summary()["completed"], 1)


if __name__ == "__main__":
    unittest.main(verbosity=1)

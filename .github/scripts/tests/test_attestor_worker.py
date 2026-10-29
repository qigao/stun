#!/usr/bin/env python3
"""Offline integration tests: external App worker, not a live issuer."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

SCRIPTS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS))
from attestor_inbox import DurableWebhookInbox
from attestor_worker import (ATTESTED_CONTEXT, AttestorWorkerError, process_one)
from test_publish_required_acceptance import (
    good_responses, pr_target, completed_workflow, REPO, SHA
)

ROOT = Path(__file__).resolve().parents[3]
REPO_ID = 1070504466
APP_INSTALLATION = 246800
APP_BOT_ID = 998877
DELIVERY = "ed7fd55c-4cbe-4cf7-861d-fb7b7de57a50"


def signed_admitted_event(event=None):
    # These records simulate the OUTPUT of the already-tested signed M0
    # admission module, not a raw unsigned HTTP payload accepted in production.
    event = copy.deepcopy(event or pr_target())
    event["repository"] = {"full_name": REPO, "id": REPO_ID}
    event["installation"] = {"id": APP_INSTALLATION}
    if event["_event_name"] == "workflow_run":
        event["workflow_run"].setdefault(
            "path", ".github/workflows/runtime-conformance.yml"
        )
    return event


def body_digest(event):
    encoded = json.dumps(event, sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha256(encoded).hexdigest()


class DedicatedWorkerTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="stun-app-worker-")
        self.addCleanup(self.tmp.cleanup)
        self.db = DurableWebhookInbox(Path(self.tmp.name) / "events.db")
        self.addCleanup(self.db.close)
        self.responses = good_responses()
        self.get_calls = []
        self.writes = []

    def get(self, path):
        self.get_calls.append(path)
        if path not in self.responses:
            raise KeyError("unexpected API endpoint: " + path)
        return copy.deepcopy(self.responses[path])

    def writer(self, sha, state, description, number, context):
        self.writes.append((sha, state, description, number, context))
        return {
            "sha": sha, "context": context, "state": state,
            "creator": {"id": APP_BOT_ID, "type": "Bot"}
        }

    def enqueue(self, event=None, delivery=DELIVERY):
        event = signed_admitted_event(event)
        self.assertTrue(self.db.enqueue(delivery, body_digest(event), event))
        return event

    def process(self, **kwargs):
        settings = {
            "repository": REPO, "repository_id": REPO_ID,
            "installation_id": APP_INSTALLATION,
            "issuer_bot_id": APP_BOT_ID, "trusted_policy_root": ROOT,
            "github_get": self.get, "write_app_status": self.writer,
        }
        settings.update(kwargs)
        return process_one(self.db, "worker-1", **settings)

    def test_success_uses_new_app_context_not_actions_context(self):
        self.enqueue()
        outcome = self.process()
        self.assertEqual(outcome[0][1], "success")
        self.assertEqual(self.db.summary()["completed"], 1)
        self.assertEqual(len(self.writes), 1)
        self.assertEqual(self.writes[0][4], "Stun attested acceptance")
        self.assertNotEqual(self.writes[0][4], "Stun required acceptance")
        self.assertEqual(self.writes[0][0], SHA)

    def test_negative_native_evidence_publishes_failure_but_finishes_item(self):
        self.enqueue()
        self.responses[
            "/actions/runs/101/jobs?filter=latest&per_page=100&page=1"
        ]["jobs"][0]["conclusion"] = "skipped"
        outcome = self.process()
        self.assertEqual(outcome[0][1], "failure")
        self.assertEqual(self.writes[0][1], "failure")
        self.assertEqual(self.db.summary()["completed"], 1)

    def test_missing_ci_full_label_publishes_fail_closed_status(self):
        self.enqueue()
        self.responses["/pulls/5"]["labels"] = []
        outcome = self.process()
        self.assertEqual(outcome[0][1], "failure")
        self.assertEqual(self.db.summary()["completed"], 1)

    def test_workflow_run_with_empty_pull_requests_is_reconciled_from_api(self):
        self.enqueue(completed_workflow())
        result = self.process()
        self.assertEqual(result[0][1], "success")
        self.assertEqual(self.writes[0][0], SHA)
        self.assertTrue(self.get_calls)

    def test_stale_or_closed_pr_does_not_replay_an_old_success(self):
        event = pr_target(sha="b" * 40)
        self.enqueue(event)
        self.assertEqual(self.process(), [])
        self.assertEqual(self.writes, [])
        self.assertEqual(self.db.summary()["completed"], 1)

    def test_empty_queue_writes_nothing(self):
        self.assertIsNone(self.process())
        self.assertEqual(self.writes, [])

    def test_missing_dedicated_app_identity_fails_before_claim(self):
        self.enqueue()
        for value in (0, False, None):
            with self.subTest(value=value), self.assertRaises(AttestorWorkerError):
                self.process(issuer_bot_id=value)
        self.assertEqual(self.db.summary()["pending"], 1)
        self.assertEqual(self.writes, [])

    def test_wrong_installed_app_identity_is_terminal_failure(self):
        event = signed_admitted_event()
        event["installation"]["id"] = APP_INSTALLATION + 1
        self.enqueue(event)
        with self.assertRaises(AttestorWorkerError):
            self.process()
        self.assertEqual(self.db.summary()["failed"], 1)
        self.assertEqual(self.writes, [])

    def test_tampered_queued_workflow_name_is_terminal_failure(self):
        event = signed_admitted_event(completed_workflow())
        event["workflow_run"]["path"] = ".github/workflows/unknown.yml"
        self.enqueue(event)
        with self.assertRaises(AttestorWorkerError):
            self.process()
        self.assertEqual(self.db.summary()["failed"], 1)
        self.assertEqual(self.get_calls, [])

    def test_wrong_pr_base_is_terminal_failure(self):
        event = signed_admitted_event()
        event["pull_request"]["base"]["ref"] = "feature"
        self.enqueue(event)
        with self.assertRaises(AttestorWorkerError):
            self.process()
        self.assertEqual(self.db.summary()["failed"], 1)

    def test_status_from_wrong_bot_is_rejected_not_completed(self):
        self.enqueue()
        def wrong_bot(sha, state, msg, number, context):
            return {"sha": sha, "state": state, "context": context,
                    "creator": {"id": APP_BOT_ID + 1, "type": "Bot"}}
        with self.assertRaises(AttestorWorkerError):
            self.process(write_app_status=wrong_bot)
        self.assertEqual(self.db.summary()["failed"], 1)
        self.assertEqual(self.db.summary()["completed"], 0)
        self.assertIsNone(self.db.claim_next("retry-worker"))

    def test_wrong_status_context_is_rejected(self):
        self.enqueue()
        def wrong_status(sha, state, msg, number, context):
            return {"sha": sha, "state": state,
                    "context": "Stun required acceptance",
                    "creator": {"id": APP_BOT_ID, "type": "Bot"}}
        with self.assertRaises(AttestorWorkerError):
            self.process(write_app_status=wrong_status)
        self.assertEqual(self.db.summary()["failed"], 1)

    def test_writer_api_failure_is_terminal_and_no_auto_retry(self):
        self.enqueue()
        def fail(*_):
            raise OSError("installation token expired; manual recovery required")
        with self.assertRaises(OSError):
            self.process(write_app_status=fail)
        self.assertEqual(self.db.summary()["failed"], 1)
        self.assertEqual(self.db.summary()["pending"], 0)
        self.assertEqual(self.writes, [])

    def test_malformed_status_response_is_terminal_failure(self):
        self.enqueue()
        with self.assertRaises(AttestorWorkerError):
            self.process(write_app_status=lambda *_: {})
        self.assertEqual(self.db.summary()["failed"], 1)

    def test_policy_must_be_pinned_absolute_default_branch_tree(self):
        self.enqueue()
        with self.assertRaises(AttestorWorkerError):
            self.process(trusted_policy_root=Path("relative/checkout"))
        self.assertEqual(self.db.summary()["pending"], 1)
        self.assertEqual(self.writes, [])

    def test_nosuch_event_kind_is_terminal_failure(self):
        event = signed_admitted_event()
        event["_event_name"] = "push"
        self.enqueue(event)
        with self.assertRaises(AttestorWorkerError):
            self.process()
        self.assertEqual(self.db.summary()["failed"], 1)


if __name__ == "__main__":
    unittest.main(verbosity=1)

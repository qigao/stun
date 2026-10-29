#!/usr/bin/env python3
"""Offline, deterministic negative and positive exact-head CI acceptance tests."""
import copy
import importlib.util
from pathlib import Path
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "verify_exact_head_ci.py"
spec = importlib.util.spec_from_file_location("verify_exact_head_ci", SCRIPT)
verifier = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verifier)

HEAD = "a" * 40
WORKFLOW = ".github/workflows/runtime-conformance.yml"
JOB = "Linux / focused installed-SDK ASan/UBSan"
REQUIREMENTS = {WORKFLOW: [JOB]}


def good_snapshot():
    return {
        "head_sha": HEAD,
        "head_sha_after": HEAD,
        "labels": ["ci/full"],
        "runs": [{
            "id": 100,
            "run_attempt": 1,
            "path": WORKFLOW,
            "head_sha": HEAD,
            "event": "pull_request",
            "status": "completed",
            "conclusion": "success",
            "jobs": [{
                "name": JOB,
                "head_sha": HEAD,
                "status": "completed",
                "conclusion": "success",
            }],
        }],
    }


class AcceptanceTests(unittest.TestCase):
    def test_exact_success(self):
        self.assertEqual(verifier.verify_snapshot(good_snapshot(), REQUIREMENTS), [])

    def test_label_required(self):
        v = good_snapshot()
        v["labels"] = []
        self.assertTrue(verifier.verify_snapshot(v, REQUIREMENTS))

    def test_head_changes_during_audit(self):
        v = good_snapshot()
        v["head_sha_after"] = "b" * 40
        self.assertTrue(verifier.verify_snapshot(v, REQUIREMENTS))

    def test_old_head_or_manual_dispatch_rejected(self):
        for key, value in [("head_sha", "b" * 40), ("event", "workflow_dispatch")]:
            v = good_snapshot()
            v["runs"][0][key] = value
            self.assertTrue(verifier.verify_snapshot(v, REQUIREMENTS), (key, value))

    def test_all_non_success_conclusions_rejected(self):
        for conclusion in ("skipped", "neutral", "cancelled", "failure", "timed_out", None):
            v = good_snapshot()
            v["runs"][0]["jobs"][0]["conclusion"] = conclusion
            self.assertTrue(verifier.verify_snapshot(v, REQUIREMENTS), conclusion)

    def test_workflow_not_completed_success(self):
        for status, conclusion in [("in_progress", None), ("queued", None),
                                   ("completed", "neutral"), ("completed", "cancelled")]:
            v = good_snapshot()
            v["runs"][0]["status"] = status
            v["runs"][0]["conclusion"] = conclusion
            self.assertTrue(verifier.verify_snapshot(v, REQUIREMENTS))

    def test_missing_or_duplicate_required_job(self):
        v = good_snapshot()
        v["runs"][0]["jobs"] = []
        self.assertTrue(verifier.verify_snapshot(v, REQUIREMENTS))
        v = good_snapshot()
        v["runs"][0]["jobs"].append(copy.deepcopy(v["runs"][0]["jobs"][0]))
        self.assertTrue(verifier.verify_snapshot(v, REQUIREMENTS))

    def test_wrong_job_sha_and_noncomplete_status(self):
        v = good_snapshot()
        v["runs"][0]["jobs"][0]["head_sha"] = "b" * 40
        self.assertTrue(verifier.verify_snapshot(v, REQUIREMENTS))
        v = good_snapshot()
        v["runs"][0]["jobs"][0]["status"] = "queued"
        self.assertTrue(verifier.verify_snapshot(v, REQUIREMENTS))

    def test_latest_run_supersedes_old_green(self):
        v = good_snapshot()
        future = copy.deepcopy(v["runs"][0])
        future["id"] = 101
        future["status"] = "queued"
        future["conclusion"] = None
        v["runs"].append(future)
        self.assertTrue(verifier.verify_snapshot(v, REQUIREMENTS))

    def test_missing_workflow_or_evidence(self):
        v = good_snapshot()
        v["runs"] = []
        self.assertTrue(verifier.verify_snapshot(v, REQUIREMENTS))
        v = good_snapshot()
        del v["runs"][0]["jobs"]
        self.assertTrue(verifier.verify_snapshot(v, REQUIREMENTS))

    def test_invalid_sha(self):
        v = good_snapshot()
        v["head_sha"] = ""
        self.assertTrue(verifier.verify_snapshot(v, REQUIREMENTS))

    def test_requirements_reject_empty_and_duplicates(self):
        with self.assertRaises(ValueError):
            verifier.parse_requirements({"required_workflows": []})
        with self.assertRaises(ValueError):
            verifier.parse_requirements({"required_workflows": [
                {"path": WORKFLOW, "jobs": [JOB, JOB]},
            ]})
        with self.assertRaises(ValueError):
            verifier.parse_requirements({"required_workflows": [
                {"path": "../arbitrary.yml", "jobs": [JOB]},
            ]})
        self.assertEqual(
            verifier.parse_requirements({"required_workflows": [
                {"path": WORKFLOW, "jobs": [JOB]},
            ]}),
            REQUIREMENTS
        )

    def test_collection_uses_exact_head_and_checks_again(self):
        responses = {
            "/pulls/7": [
                {"head": {"sha": HEAD}, "labels": [{"name": "ci/full"}]},
                {"head": {"sha": HEAD}, "labels": [{"name": "ci/full"}]},
            ],
            "/actions/runs?head_sha=" + HEAD + "&event=pull_request&per_page=100&page=1": {
                "workflow_runs": [copy.deepcopy(good_snapshot()["runs"][0])]
            },
            "/actions/runs/100/jobs?filter=latest&per_page=100&page=1": {
                "jobs": copy.deepcopy(good_snapshot()["runs"][0]["jobs"])
            },
        }
        calls = []
        def get(path):
            calls.append(path)
            value = responses[path]
            if isinstance(value, list):
                return value.pop(0)
            return copy.deepcopy(value)

        snapshot = verifier.collect_snapshot(get, 7, REQUIREMENTS)
        self.assertEqual(calls.count("/pulls/7"), 2)
        self.assertEqual(verifier.verify_snapshot(snapshot, REQUIREMENTS), [])

    def test_pagination_bound_fails_closed(self):
        def too_many(_):
            return {"workflow_runs": [{"id": 1}] * 100}
        with self.assertRaises(ValueError):
            verifier.pages(too_many, "/actions/runs", "workflow_runs", limit=2)


if __name__ == "__main__":
    unittest.main(verbosity=1)

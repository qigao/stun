#!/usr/bin/env python3
"""No-network contract tests for trusted event-driven commit-status publisher."""
import copy
import importlib.util
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[3]
SCRIPT_DIR = ROOT / ".github/scripts"
sys.path.insert(0, str(SCRIPT_DIR))
spec = importlib.util.spec_from_file_location(
    "publish_required_acceptance", SCRIPT_DIR / "publish_required_acceptance.py"
)
publisher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publisher)

SHA = "a" * 40
REPO = "qigao/stun"
PATH = ".github/scripts/publish_required_acceptance.py"


def pr(number=5, sha=SHA, labels=("ci/full",)):
    return {
        "number": number, "state": "open",
        "head": {"sha": sha},
        "base": {"ref": "master", "repo": {"full_name": REPO}},
        "labels": [{"name": name} for name in labels],
    }


def run(number, workflow):
    return {
        "id": number, "run_attempt": 1, "path": workflow,
        "event": "pull_request", "head_sha": SHA,
        "status": "completed", "conclusion": "success",
        "created_at": "2026-10-29T01:05:00Z",
    }


def job(name, conclusion="success", sha=SHA):
    return {
        "name": name, "status": "completed", "conclusion": conclusion,
        "head_sha": sha,
        "started_at": "2026-10-29T01:05:00Z",
    }


def good_responses():
    return {
        "/pulls?state=open&per_page=100&page=1": [pr()],
        "/pulls/5": pr(),
        "/pulls/5/files?per_page=100&page=1": [
            {"filename": PATH, "status": "added"}
        ],
        "/issues/5/events?per_page=100&page=1": [
            {"event": "labeled", "label": {"name": "ci/full"},
             "created_at": "2026-10-29T01:00:00Z"},
        ],
        "/actions/runs?head_sha=" + SHA + "&event=pull_request&per_page=100&page=1": {
            "workflow_runs": [
                run(100, ".github/workflows/public-dependency-contract.yml"),
                run(101, ".github/workflows/runtime-conformance.yml"),
            ]
        },
        "/actions/runs/100/jobs?filter=latest&per_page=100&page=1": {
            "jobs": [job("Repository sole C11 parser contract")]
        },
        "/actions/runs/101/jobs?filter=latest&per_page=100&page=1": {
            "jobs": [job("Linux / focused installed-SDK ASan/UBSan")]
        },
    }


def pr_target(sha=SHA, label=None):
    e = {
        "_event_name": "pull_request_target", "action": "synchronize",
        "number": 5, "pull_request": pr(sha=sha)
    }
    if label is not None:
        e["action"] = "labeled"
        e["label"] = {"name": label}
    return e


def completed_workflow(sha=SHA):
    return {
        "_event_name": "workflow_run", "action": "completed",
        "workflow_run": {
            "event": "pull_request", "head_sha": sha,
            "pull_requests": [], "id": 101,
        },
    }


def simulate(event=None, changes=None):
    responses = good_responses()
    if changes:
        changes(responses)
    posted = []
    calls = []
    def get(path):
        calls.append(path)
        if path not in responses:
            raise AssertionError("Unexpected API path: " + path)
        return copy.deepcopy(responses[path])

    def post(sha, state, description, number):
        posted.append((sha, state, description, number))

    outcome = publisher.evaluate_event(
        event or pr_target(), get, post, REPO, ROOT
    )
    return outcome, posted, calls


class PublisherTests(unittest.TestCase):
    def test_success_writes_only_one_exact_commit_status(self):
        result, posts, _ = simulate()
        self.assertEqual(result[0][1], "success")
        self.assertEqual(len(posts), 1)
        self.assertEqual(posts[0][0:2], (SHA, "success"))
        self.assertEqual(posts[0][3], 5)

    def test_workflow_run_without_pull_requests_resolves_head(self):
        result, posts, calls = simulate(completed_workflow())
        self.assertEqual(result[0][1], "success")
        self.assertIn("/pulls?state=open&per_page=100&page=1", calls)
        self.assertEqual(len(posts), 1)

    def test_queued_required_job_cannot_be_green(self):
        def change(d):
            d["/actions/runs/101/jobs?filter=latest&per_page=100&page=1"][
                "jobs"
            ][0]["status"] = "queued"
        result, posts, _ = simulate(changes=change)
        self.assertEqual(result[0][1], "failure")
        self.assertEqual(posts[0][1], "failure")

    def test_skipped_neutral_cancelled_failed_jobs_cannot_be_green(self):
        for conclusion in ("skipped", "neutral", "cancelled", "failure", None):
            def change(d):
                d["/actions/runs/101/jobs?filter=latest&per_page=100&page=1"][
                    "jobs"
                ][0]["conclusion"] = conclusion
            with self.subTest(conclusion=conclusion):
                _, posts, _ = simulate(changes=change)
                self.assertEqual(posts[0][1], "failure")

    def test_no_ci_full_label_is_failure(self):
        def change(d):
            d["/pulls/5"]["labels"] = []
        _, posts, _ = simulate(changes=change)
        self.assertEqual(posts[0][1], "failure")

    def test_pre_label_green_runs_are_not_accepted(self):
        def change(d):
            d["/issues/5/events?per_page=100&page=1"][0][
                "created_at"
            ] = "2026-10-29T01:10:00Z"
        _, posts, _ = simulate(changes=change)
        self.assertEqual(posts[0][1], "failure")
        self.assertIn("predates ci/full", posts[0][2])

    def test_label_without_event_provenance_is_failure(self):
        def change(d):
            d["/issues/5/events?per_page=100&page=1"] = []
        _, posts, _ = simulate(changes=change)
        self.assertEqual(posts[0][1], "failure")

    def test_latest_ci_full_relabel_requires_new_tests(self):
        def change(d):
            d["/issues/5/events?per_page=100&page=1"].append({
                "event": "labeled", "label": {"name": "ci/full"},
                "created_at": "2026-10-29T01:15:00Z",
            })
        _, posts, _ = simulate(changes=change)
        self.assertEqual(posts[0][1], "failure")

    def test_rerun_after_label_is_valid_even_when_run_is_older(self):
        def change(d):
            for run in d["/actions/runs?head_sha=" + SHA +
                         "&event=pull_request&per_page=100&page=1"]["workflow_runs"]:
                run["created_at"] = "2026-10-29T00:30:00Z"
                run["run_attempt"] = 2
            # The actual required jobs were executed after label admission.
        _, posts, _ = simulate(changes=change)
        self.assertEqual(posts[0][1], "success")

    def test_missing_job_start_time_fails_closed(self):
        def change(d):
            del d["/actions/runs/101/jobs?filter=latest&per_page=100&page=1"]["jobs"][0]["started_at"]
        _, posts, _ = simulate(changes=change)
        self.assertEqual(posts[0][1], "failure")

    def test_exact_timestamp_is_valid(self):
        def change(d):
            d["/issues/5/events?per_page=100&page=1"][0][
                "created_at"
            ] = "2026-10-29T01:05:00Z"
        _, posts, _ = simulate(changes=change)
        self.assertEqual(posts[0][1], "success")

    def test_missing_workflow_failure(self):
        def change(d):
            d["/actions/runs?head_sha=" + SHA +
              "&event=pull_request&per_page=100&page=1"]["workflow_runs"].pop()
        _, posts, _ = simulate(changes=change)
        self.assertEqual(posts[0][1], "failure")

    def test_unknown_source_root_does_not_fallback(self):
        def change(d):
            d["/pulls/5/files?per_page=100&page=1"][0][
                "filename"
            ] = "unowned/source.c"
        _, posts, _ = simulate(changes=change)
        self.assertEqual(posts[0][1], "failure")

    def test_newer_queue_supersedes_old_green(self):
        def change(d):
            d["/actions/runs?head_sha=" + SHA +
              "&event=pull_request&per_page=100&page=1"][
                "workflow_runs"
            ].append({
                **run(102, ".github/workflows/runtime-conformance.yml"),
                "status": "queued", "conclusion": None
            })
        _, posts, _ = simulate(changes=change)
        self.assertEqual(posts[0][1], "failure")

    def test_stale_event_or_unrelated_label_does_not_publish(self):
        for event in (
            pr_target(sha="b" * 40),
            pr_target(label="documentation"),
            {"_event_name": "workflow_run", "action": "completed",
             "workflow_run": {"head_sha": SHA, "event": "push"}},
        ):
            with self.subTest(event=event):
                result, posts, _ = simulate(event=event)
                self.assertEqual(result, [])
                self.assertEqual(posts, [])

    def test_changed_head_at_last_read_is_never_success(self):
        counts = [0]
        responses = good_responses()
        published = []
        def get(path):
            if path == "/pulls/5":
                counts[0] += 1
                if counts[0] >= 4:
                    return pr(sha="b" * 40)
            return copy.deepcopy(responses[path])
        result = publisher.evaluate_event(
            pr_target(), get, lambda *args: published.append(args), REPO, ROOT
        )
        self.assertEqual(result, [])
        self.assertEqual(published, [])

    def test_same_sha_second_pr_must_not_bypass_missing_label(self):
        responses = good_responses()
        responses["/pulls?state=open&per_page=100&page=1"].append(pr(number=6))
        responses["/pulls/6"] = pr(number=6, labels=())
        responses["/pulls/6/files?per_page=100&page=1"] = [
            {"filename": PATH, "status": "modified"}
        ]
        published = []
        publisher.evaluate_event(
            pr_target(),
            lambda path: copy.deepcopy(responses[path]),
            lambda *args: published.append(args), REPO, ROOT
        )
        self.assertEqual(len(published), 1)
        self.assertEqual(published[0][1], "failure")

    def test_fork_or_other_base_not_selected(self):
        def change(d):
            d["/pulls?state=open&per_page=100&page=1"][0][
                "base"
            ]["ref"] = "dev"
        result, posts, _ = simulate(changes=change)
        self.assertEqual(result, [])
        self.assertEqual(posts, [])

    def test_status_message_is_bounded(self):
        message = publisher.status_message("failure", ["x" * 500])
        self.assertLessEqual(len(message), 140)
        self.assertNotIn("\n", message)

    def test_read_event_requires_source_identity(self):
        self.assertIsNone(publisher.read_event(
            {"_event_name": "workflow_run", "action": "completed",
             "workflow_run": {"event": "push", "head_sha": SHA}}
        ))
        with self.assertRaises(ValueError):
            publisher.read_event(
                {"_event_name": "pull_request_target",
                 "action": "opened", "pull_request": {}}
            )

    def test_unsupported_event_never_publishes(self):
        self.assertIsNone(publisher.read_event(
            {"_event_name": "issue_comment", "action": "created"}
        ))


if __name__ == "__main__":
    unittest.main(verbosity=1)

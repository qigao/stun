#!/usr/bin/env python3
"""Trusted event-driven publisher for the Stun required acceptance STATUS.

Runs from DEFAULT-BRANCH source only; NEVER checkout or execute PR files.
Writes one stable commit status to the exact PR HEAD; does not execute native
tests or poll runners. The status is advisory until a master ruleset requires
its context. A successful workflow summary with skipped required jobs fails.
"""
import json
import os
from pathlib import Path
import re
import sys
from urllib.request import Request, urlopen

from ci_required_matrix import load_workflow_requirements, pr_changed_paths
from verify_exact_head_ci import (
    SHA_RE, collect_snapshot, github_get, pages, verify_snapshot,
)

CONTEXT = "Stun required acceptance"


def valid_repo(repo):
    return isinstance(repo, str) and re.fullmatch(
        r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repo
    ) is not None


def read_event(event):
    """Return (event head SHA, PR number or None), or None if irrelevant."""
    if not isinstance(event, dict):
        raise ValueError("malformed GitHub event")
    name, payload = event.get("_event_name"), event
    if name == "pull_request_target":
        if payload.get("action") not in (
            "opened", "synchronize", "reopened", "labeled",
            "unlabeled", "edited",
        ):
            return None
        if payload.get("action") in ("labeled", "unlabeled"):
            if payload.get("label", {}).get("name") != "ci/full":
                return None
        pr = payload.get("pull_request", {})
        sha = pr.get("head", {}).get("sha")
        number = pr.get("number", payload.get("number"))
        if not isinstance(number, int) or number < 1 or not isinstance(sha, str):
            raise ValueError("malformed PR event identity")
        return sha, number
    if name == "workflow_run":
        workflow = payload.get("workflow_run", {})
        if (payload.get("action") not in ("requested", "completed") or
                workflow.get("event") != "pull_request"):
            return None
        sha = workflow.get("head_sha")
        if not isinstance(sha, str):
            raise ValueError("missing workflow run HEAD")
        return sha, None
    return None


def open_prs_at_sha(get, repo, sha):
    """A workflow_run may provide an empty pull_requests array."""
    result = []
    for item in pages(get, "/pulls?state=open", None):
        if not isinstance(item, dict):
            raise ValueError("malformed pull request list")
        base = item.get("base", {})
        if (item.get("head", {}).get("sha") == sha
                and base.get("ref") == "master"
                and base.get("repo", {}).get("full_name") == repo):
            number = item.get("number")
            if not isinstance(number, int) or number <= 0:
                raise ValueError("invalid PR identifier in list")
            result.append(number)
    return sorted(set(result))


def latest_ci_full_label_time(get, number):
    """Require real ci/full admission chronology, not only current label."""
    from datetime import datetime, timezone
    events = pages(get, f"/issues/{number}/events", None, limit=20)
    timestamps = []
    for item in events:
        if not isinstance(item, dict):
            raise ValueError("malformed issue label event")
        if item.get("event") != "labeled":
            continue
        if item.get("label", {}).get("name") != "ci/full":
            continue
        stamp = item.get("created_at")
        if not isinstance(stamp, str):
            raise ValueError("ci/full label event missing timestamp")
        timestamps.append(datetime.fromisoformat(
            stamp.replace("Z", "+00:00")
        ).astimezone(timezone.utc))
    if not timestamps:
        raise ValueError("ci/full label admission event not found")
    return max(timestamps)


def ci_full_freshness_errors(snapshot, requirements, label_time):
    """Only real required jobs admitted since ci/full may satisfy the gate."""
    from datetime import datetime, timezone
    errors = []
    sha = snapshot.get("head_sha")
    for path, names in requirements.items():
        candidates = [r for r in snapshot.get("runs", [])
                      if r.get("path") == path and r.get("head_sha") == sha
                      and r.get("event") == "pull_request"]
        if not candidates:
            continue  # M0 rejects missing workflow.
        latest = max(candidates, key=lambda r: (
            int(r.get("id", 0)), int(r.get("run_attempt", 0))
        ))
        if (latest.get("status") != "completed" or
                latest.get("conclusion") != "success"):
            continue  # M0 rejects unfinished or unsuccessful runs.
        for name in names:
            matched = [j for j in latest.get("jobs", [])
                       if j.get("name") == name]
            if len(matched) != 1:
                continue  # M0 rejects missing or duplicate jobs.
            stamp = matched[0].get("started_at")
            if not isinstance(stamp, str):
                errors.append(f"{path}: job {name!r} has no start timestamp")
                continue
            when = datetime.fromisoformat(
                stamp.replace("Z", "+00:00")
            ).astimezone(timezone.utc)
            if when < label_time:
                errors.append(f"{path}: job {name!r} predates ci/full admission")
    return errors


def assess_one(get, number, repo_root, expected_sha):
    """Recheck PR identity before and after evidence and policy selection."""
    before = get(f"/pulls/{number}")
    sha = before.get("head", {}).get("sha")
    if not isinstance(sha, str) or not SHA_RE.fullmatch(sha):
        raise ValueError("invalid PR source SHA")
    if sha != expected_sha:
        raise ValueError("stale PR source HEAD")
    if before.get("state") != "open":
        raise ValueError("PR already closed")
    if before.get("base", {}).get("ref") != "master":
        raise ValueError("PR does not target master")
    paths = pr_changed_paths(get, number, pages)
    requirements = load_workflow_requirements(paths, repo_root)
    snapshot = collect_snapshot(get, number, requirements)
    errors = verify_snapshot(snapshot, requirements)
    if "ci/full" in snapshot.get("labels", []):
        admission_time = latest_ci_full_label_time(get, number)
        errors.extend(ci_full_freshness_errors(
            snapshot, requirements, admission_time
        ))
    if snapshot.get("head_sha") != sha:
        errors.append("PR changed while trusted requirements were selected")
    final = get(f"/pulls/{number}")
    if (final.get("head", {}).get("sha") != sha or
            final.get("state") != "open" or
            final.get("base", {}).get("ref") != "master"):
        raise ValueError("PR changed before publishing status")
    return errors


def status_message(state, errors):
    if state == "success":
        return "Current PR HEAD: all required native jobs executed SUCCESS"
    reason = str(errors[0] if errors else "missing required CI evidence")
    reason = " ".join(reason.split())
    # GitHub status descriptions must be no longer than 140 characters.
    return ("REJECT: " + reason)[:138]


def evaluate_event(event, get, publish, repo, repo_root):
    """Pure-ish integration seam for deterministic tests: no hidden fallback."""
    selected = read_event(event)
    if selected is None:
        return []
    sha, number = selected
    if not SHA_RE.fullmatch(sha):
        raise ValueError("invalid event HEAD SHA")
    numbers = open_prs_at_sha(get, repo, sha)
    if number is not None and number not in numbers:
        # Outdated pull_request_target delivery cannot overwrite a new HEAD.
        return []
    if not numbers:
        return []
    errors = []
    for pr_number in numbers:
        try:
            errors.extend(f"PR #{pr_number}: {s}" for s in assess_one(
                get, pr_number, repo_root, sha
            ))
        except (OSError, ValueError, TypeError, KeyError) as exc:
            # A changed head is never re-labelled using its old SHA.
            if "stale PR source HEAD" in str(exc) or "changed before publishing" in str(exc):
                return []
            errors.append(f"PR #{pr_number}: {exc}")

    # Reject all incomplete/uncertain evidence. After each upstream
    # workflow_run completes, recompute from GitHub, never from artifacts.
    state = "failure" if errors else "success"
    # Last-minute check before writing to avoid race with synchronize.
    still_open = open_prs_at_sha(get, repo, sha)
    if still_open != numbers:
        return []
    publish(sha, state, status_message(state, errors), numbers[0])
    return [(sha, state, tuple(errors))]


def post_status(repo, sha, state, description, number, token):
    url = f"https://api.github.com/repos/{repo}/statuses/{sha}"
    body = json.dumps({
        "state": state,
        "context": CONTEXT,
        "description": description,
        "target_url": f"https://github.com/{repo}/pull/{number}/checks",
    }).encode("utf-8")
    request = Request(url, data=body, method="POST", headers={
        "Accept": "application/vnd.github+json",
        "Authorization": "Bearer " + token,
        "Content-Type": "application/json",
        "X-GitHub-Api-Version": "2022-11-28",
    })
    with urlopen(request, timeout=30) as response:
        payload = json.load(response)
    if payload.get("context") != CONTEXT or payload.get("state") != state:
        raise ValueError("GitHub did not confirm exact required status")
    return payload


def main():
    repo = os.environ.get("GITHUB_REPOSITORY")
    token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
    event_name = os.environ.get("GITHUB_EVENT_NAME")
    event_path = os.environ.get("GITHUB_EVENT_PATH")
    if not valid_repo(repo) or not token or not event_name or not event_path:
        raise SystemExit("missing trusted GitHub workflow environment")
    with open(event_path, encoding="utf-8") as stream:
        event = json.load(stream)
    event["_event_name"] = event_name
    root = Path(__file__).resolve().parents[2]
    get = lambda endpoint: github_get(repo, endpoint, token)
    publish = lambda sha, state, message, number: post_status(
        repo, sha, state, message, number, token
    )
    try:
        outcomes = evaluate_event(event, get, publish, repo, root)
    except (OSError, ValueError, TypeError, KeyError) as exc:
        # Never manufacture SUCCESS after a collection or publication error.
        print("REJECT required acceptance publication:", exc, file=sys.stderr)
        return 1
    for sha, state, errors in outcomes:
        print(f"{CONTEXT}: {state} on {sha}; {len(errors)} unmet conditions")
    return 0


if __name__ == "__main__":
    sys.exit(main())

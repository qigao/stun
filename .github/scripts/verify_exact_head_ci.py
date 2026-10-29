#!/usr/bin/env python3
"""Fail-closed exact-PR-HEAD native CI evidence verifier.

This is a read-only verifier, NOT a branch-protection ruleset or a required
GitHub check. By default, the required workflow/job matrix is selected from the reviewed
PR file delta and TRUSTED default-branch workflow paths. This M1 tool is
still read-only; publishing a non-skippable required status is separate.

Run: GH_TOKEN=... python3 verify_exact_head_ci.py --repo qigao/stun --pr 123
"""
import argparse
import json
import os
import re
import sys
from pathlib import Path
from urllib.parse import quote
from urllib.request import Request, urlopen

SHA_RE = re.compile(r"^[0-9a-f]{40}$")
WORKFLOW_PREFIX = ".github/workflows/"


def parse_requirements(document):
    """Return path -> required exact job names, rejecting ambiguous input."""
    rows = document.get("required_workflows")
    if not isinstance(rows, list) or not rows:
        raise ValueError("required_workflows must be a nonempty list")
    requirements = {}
    for row in rows:
        if not isinstance(row, dict):
            raise ValueError("workflow requirement must be an object")
        path, names = row.get("path"), row.get("jobs")
        if (not isinstance(path, str) or
                not path.startswith(WORKFLOW_PREFIX) or
                not path.endswith((".yml", ".yaml")) or
                ".." in path or
                not isinstance(names, list) or not names or
                any(not isinstance(n, str) or not n.strip() for n in names) or
                len(names) != len(set(names)) or path in requirements):
            raise ValueError("invalid/duplicate workflow or required job")
        requirements[path] = names
    return requirements


def verify_snapshot(snapshot, requirements):
    """Return failure explanations; empty list means strict acceptance."""
    errors = []
    sha = snapshot.get("head_sha")
    if not isinstance(sha, str) or not SHA_RE.fullmatch(sha):
        return ["missing or invalid exact PR head SHA"]
    if snapshot.get("head_sha_after") != sha:
        errors.append("PR head changed during evidence collection")
    if "ci/full" not in snapshot.get("labels", []):
        errors.append("ci/full label absent")
    if not isinstance(snapshot.get("runs"), list):
        return errors + ["workflow evidence missing"]

    for path, names in requirements.items():
        candidates = [
            run for run in snapshot["runs"]
            if run.get("path") == path and run.get("head_sha") == sha
            and run.get("event") == "pull_request"
        ]
        if not candidates:
            errors.append(f"{path}: no pull_request run for exact head")
            continue

        # A fresh pending/cancelled run must not be hidden behind an older
        # green run on the same source SHA.
        latest = max(candidates, key=lambda r: (int(r.get("id", 0)),
                                                 int(r.get("run_attempt", 0))))
        if latest.get("status") != "completed" or latest.get("conclusion") != "success":
            errors.append(f"{path}: latest run not completed SUCCESS")
            continue

        jobs = latest.get("jobs")
        if not isinstance(jobs, list):
            errors.append(f"{path}: actual job evidence absent")
            continue
        for name in names:
            matched = [j for j in jobs if j.get("name") == name]
            if len(matched) != 1:
                errors.append(f"{path}: required job {name!r} missing or duplicate")
            elif (matched[0].get("status") != "completed" or
                  matched[0].get("conclusion") != "success" or
                  matched[0].get("head_sha") != sha):
                errors.append(f"{path}: required job {name!r} not executed SUCCESS")
    return errors


def github_get(repo, endpoint, token):
    url = "https://api.github.com/repos/" + repo + endpoint
    headers = {
        "Accept": "application/vnd.github+json",
        "Authorization": "Bearer " + token,
        "X-GitHub-Api-Version": "2022-11-28",
    }
    with urlopen(Request(url, headers=headers), timeout=30) as response:
        return json.load(response)


def pages(get, path, member, limit=10):
    values = []
    for page in range(1, limit + 1):
        sep = "&" if "?" in path else "?"
        response = get(f"{path}{sep}per_page=100&page={page}")
        rows = (response if member is None and isinstance(response, list)
                else response.get(member, []) if isinstance(response, dict) else [])
        if not isinstance(rows, list):
            raise ValueError("malformed GitHub pagination response")
        values.extend(rows)
        if len(rows) < 100:
            return values
    raise ValueError("GitHub pagination limit reached; refusing partial audit")


def collect_snapshot(get, pr_number, requirements):
    pr = get(f"/pulls/{pr_number}")
    sha = pr.get("head", {}).get("sha")
    if not isinstance(sha, str) or not SHA_RE.fullmatch(sha):
        raise ValueError("PR HEAD missing or malformed")
    labels = [r.get("name") for r in pr.get("labels", []) if isinstance(r, dict)]
    runs = pages(get, "/actions/runs?head_sha=" + quote(sha) +
                 "&event=pull_request", "workflow_runs")
    for path in requirements:
        candidates = [
            r for r in runs if r.get("path") == path and r.get("head_sha") == sha
            and r.get("event") == "pull_request"
        ]
        if candidates:
            latest = max(candidates, key=lambda r: (int(r.get("id", 0)),
                                                     int(r.get("run_attempt", 0))))
            run_id = int(latest["id"])
            latest["jobs"] = pages(get, f"/actions/runs/{run_id}/jobs?filter=latest", "jobs")
    after = get(f"/pulls/{pr_number}").get("head", {}).get("sha")
    return {
        "head_sha": sha,
        "head_sha_after": after,
        "labels": labels,
        "runs": runs,
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", required=True)
    parser.add_argument("--pr", type=int, required=True)
    parser.add_argument("--requirements", help="manual reviewed manifest; omit for trusted source-delta policy")
    args = parser.parse_args(argv)
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", args.repo) or args.pr < 1:
        parser.error("invalid repository or PR number")
    token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
    if not token:
        parser.error("GH_TOKEN or GITHUB_TOKEN required for trusted REST evidence")
    try:
        get = lambda endpoint: github_get(args.repo, endpoint, token)
        prior = get(f"/pulls/{args.pr}").get("head", {}).get("sha")
        if args.requirements:
            # Kept only for explicit manual review, never for the required
            # status publisher. PR-supplied manifests are not trusted policy.
            with open(args.requirements, encoding="utf-8") as handle:
                requirements = parse_requirements(json.load(handle))
        else:
            from ci_required_matrix import load_workflow_requirements, pr_changed_paths
            paths = pr_changed_paths(get, args.pr, pages)
            requirements = load_workflow_requirements(
                paths, Path(__file__).resolve().parents[2]
            )
        snapshot = collect_snapshot(get, args.pr, requirements)
        errors = verify_snapshot(snapshot, requirements)
        if snapshot.get("head_sha") != prior:
            errors.append("PR head moved while source-delta policy was selected")
    except (OSError, ValueError, KeyError, TypeError) as exc:
        errors = ["CI evidence collection failed: " + str(exc)]
    if errors:
        for error in errors:
            print("REJECT:", error, file=sys.stderr)
        return 1
    print("ACCEPT: exact-head required jobs executed SUCCESS")
    return 0


if __name__ == "__main__":
    sys.exit(main())

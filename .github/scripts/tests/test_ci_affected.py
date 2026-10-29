#!/usr/bin/env python3
"""Deterministic tests for CI's fail-open PR/master-push scope classifier.

Runs with the standard library and a temporary Git repository: no network,
privileged token, runner cache, native SDK or third-party test dependency.
"""
import json
import os
from pathlib import Path
import subprocess
import tempfile

SCRIPT = Path(__file__).resolve().parents[1] / "ci-affected.sh"


def git(directory, *arguments):
    return subprocess.check_output(
        ["git", *arguments], cwd=directory, text=True, stderr=subprocess.DEVNULL
    ).strip()


def check(directory, event, patterns, *, event_name="pull_request"):
    event_file = directory.parent / "event.json"
    output_file = directory.parent / "github-output.txt"
    event_file.write_text(json.dumps(event) + "\n", encoding="utf-8")
    output_file.write_text("", encoding="utf-8")
    env = dict(os.environ)
    env.update(
        GITHUB_EVENT_NAME=event_name,
        GITHUB_EVENT_PATH=str(event_file),
        GITHUB_OUTPUT=str(output_file),
    )
    result = subprocess.run(
        ["bash", str(SCRIPT), *patterns],
        cwd=directory,
        env=env,
        text=True,
        capture_output=True,
        check=True,
    )
    values = output_file.read_text(encoding="utf-8").strip().splitlines()
    assert len(values) == 1 and values[0] in ("affected=true", "affected=false"), (
        values,
        result.stdout,
    )
    return values[0] == "affected=true"


def synchronize(before, after):
    return {
        "action": "synchronize",
        "before": before,
        "after": after,
        "pull_request": {"head": {"sha": after}, "labels": []},
    }


def pull_request(action, base, head):
    return {
        "action": action,
        "pull_request": {
            "base": {"sha": base},
            "head": {"sha": head},
            "labels": [],
        },
    }



def verify_labeled_workflow_isolation():
    """Unrelated labels cannot cancel or consume the qualified CI lane."""
    workflows = Path(__file__).resolve().parents[2] / "workflows"
    guard = "    if: github.event.action != 'labeled' || github.event.label.name == 'ci/full'"
    suffix = "-${{ github.event.action == 'labeled' && github.event.label.name != 'ci/full' && github.run_id || 'qualification' }}"
    dispatch_suffix = "-${{ (github.event_name == 'workflow_dispatch' || (github.event.action == 'labeled' && github.event.label.name != 'ci/full')) && github.run_id || 'qualification' }}"
    master_push_and_dispatch = {"runtime-conformance.yml", "graphlayout-native.yml"}
    guarded = []
    for path in sorted(workflows.glob("*.yml")):
        source = path.read_text(encoding="utf-8")
        if "types: [opened, synchronize, reopened, labeled]" not in source:
            continue
        guarded.append(path.name)
        assert "jobs:\n  changes:\n" + guard + "\n" in source, path.name
        group = next(
            (line for line in source.splitlines() if line.startswith("  group: ")), ""
        )
        if path.name in master_push_and_dispatch:
            assert "  push:" in source and "  workflow_dispatch:" in source
            assert group.endswith(dispatch_suffix), (path.name, group)
        else:
            assert group.endswith(suffix), (path.name, group)
        assert "  cancel-in-progress: true" in source, path.name
    assert len(guarded) == 9, guarded
    assert master_push_and_dispatch.issubset(set(guarded)), guarded

def main():
    assert SCRIPT.is_file(), SCRIPT
    verify_labeled_workflow_isolation()
    with tempfile.TemporaryDirectory(prefix="stun-ci-scope-") as tmp:
        directory = Path(tmp) / "repo"
        directory.mkdir()
        git(directory, "init", "-q")
        git(directory, "config", "user.email", "ci@example.invalid")
        git(directory, "config", "user.name", "CI test")
        graphics = directory / "shared" / "gcanvas"
        charts = directory / "charts" / "mermaid" / "flowchart"
        graphics.mkdir(parents=True)
        charts.mkdir(parents=True)
        (graphics / "renderer.cpp").write_text("one\n", encoding="utf-8")
        (charts / "lexer.re").write_text("first\n", encoding="utf-8")
        git(directory, "add", ".")
        git(directory, "commit", "-qm", "baseline")
        before = git(directory, "rev-parse", "HEAD")

        (charts / "lexer.re").write_text("second\n", encoding="utf-8")
        git(directory, "commit", "-qam", "parser-only update")
        after = git(directory, "rev-parse", "HEAD")
        event = synchronize(before, after)
        assert check(directory, event, ["charts/mermaid/flowchart/*"])
        assert not check(directory, event, ["shared/gcanvas/*"])
        assert check(directory, event, ["shared/gcanvas/*", "charts/*"])

        # A parser-only push can use its dedicated native smoke instead
        # of rebuilding Mesa and the entire installed-SDK graph.
        assert not check(directory, event, [
            "charts/*", "!charts/mermaid/flowchart/lexer.re",
        ])
        assert check(directory, event, [
            "charts/*", "!charts/mermaid/flowchart/another-file.c",
        ])
        # Exclusion-only scopes are invalid, so fail open rather than skip.
        assert check(directory, event, ["!charts/mermaid/flowchart/*"])

        labeled = dict(event)
        labeled["action"] = "labeled"
        labeled["label"] = {"name": "not-ci"}
        assert not check(directory, labeled, ["shared/gcanvas/*"])
        labeled["label"] = {"name": "ci/full"}
        assert check(directory, labeled, ["shared/gcanvas/*"])

        full = synchronize(before, after)
        full["pull_request"]["labels"] = [{"name": "ci/full"}]
        assert check(directory, full, ["shared/gcanvas/*"])
        assert check(directory, full, [
            "charts/*", "!charts/mermaid/flowchart/lexer.re",
        ])
        # Initial and lifecycle PR events classify the cumulative base-to-head
        # diff instead of unconditionally starting every expensive suite.
        for action in ("opened", "reopened", "ready_for_review"):
            initial = pull_request(action, before, after)
            assert check(directory, initial, ["charts/mermaid/flowchart/*"])
            assert not check(directory, initial, ["shared/gcanvas/*"])

        # Missing metadata and unknown actions retain the fail-open contract.
        assert check(directory, {"action": "opened"}, ["shared/gcanvas/*"])
        assert check(directory, {"action": "edited"}, ["shared/gcanvas/*"])
        assert check(directory, {"action": "synchronize"}, ["shared/gcanvas/*"])
        # Master pushes use the actual before/after delta. Unrelated tests
        # are skipped, while missing SHA or checkout mismatch fails open.
        master_push = {"ref": "refs/heads/master", "before": before, "after": after}
        assert check(directory, master_push, [
            "charts/mermaid/flowchart/*",
        ], event_name="push")
        assert not check(directory, master_push, [
            "shared/gcanvas/*",
        ], event_name="push")
        assert check(directory, dict(master_push, before="0" * 40), [
            "shared/gcanvas/*",
        ], event_name="push")
        assert check(directory, dict(master_push, after="f" * 40), [
            "shared/gcanvas/*",
        ], event_name="push")
        # Dispatch remains a full, explicit qualification.
        assert check(directory, master_push, [
            "shared/gcanvas/*",
        ], event_name="workflow_dispatch")
        assert check(directory, master_push, [
            "shared/gcanvas/*",
        ], event_name="unknown_event")

        (graphics / "renderer.cpp").write_text("two\n", encoding="utf-8")
        git(directory, "commit", "-qam", "graphics-only update")
        newer = git(directory, "rev-parse", "HEAD")
        event = synchronize(after, newer)
        assert check(directory, event, ["shared/gcanvas/*"])
        assert not check(directory, event, ["charts/mermaid/flowchart/*"])

        # Push with two commits must select both changed scopes but not tango.
        cumulative_push = {"ref": "refs/heads/master", "before": before, "after": newer}
        assert check(directory, cumulative_push, ["shared/gcanvas/*"], event_name="push")
        assert check(directory, cumulative_push, ["charts/mermaid/flowchart/*"], event_name="push")
        assert not check(directory, cumulative_push, ["tango/*"], event_name="push")

        # A newly opened PR uses the full branch delta, so scopes changed by
        # separate commits are both retained in the initial qualification.
        cumulative = pull_request("opened", before, newer)
        assert check(directory, cumulative, ["shared/gcanvas/*"])
        assert check(directory, cumulative, ["charts/mermaid/flowchart/*"])

        # A move away from an owned directory must be treated as a change to
        # that directory, even when Git can recognize the rename.
        (directory / "docs").mkdir()
        git(directory, "mv", "shared/gcanvas/renderer.cpp", "docs/renderer.cpp")
        git(directory, "commit", "-qm", "move graphics source")
        newest = git(directory, "rev-parse", "HEAD")
        assert check(directory, synchronize(newer, newest), ["shared/gcanvas/*"])

    print("CI PR/master-push scope: all targeted and fail-open cases passed")


if __name__ == "__main__":
    main()

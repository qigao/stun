#!/usr/bin/env python3
"""Deterministic tests for CI's fail-open PR-update scope classifier.

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


def main():
    assert SCRIPT.is_file(), SCRIPT
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
        assert check(directory, {"action": "opened"}, ["shared/gcanvas/*"])
        assert check(directory, {"action": "synchronize"}, ["shared/gcanvas/*"])
        assert check(directory, event, ["shared/gcanvas/*"], event_name="push")

        (graphics / "renderer.cpp").write_text("two\n", encoding="utf-8")
        git(directory, "commit", "-qam", "graphics-only update")
        newer = git(directory, "rev-parse", "HEAD")
        event = synchronize(after, newer)
        assert check(directory, event, ["shared/gcanvas/*"])
        assert not check(directory, event, ["charts/mermaid/flowchart/*"])

        # A move away from an owned directory must be treated as a change to
        # that directory, even when Git can recognize the rename.
        (directory / "docs").mkdir()
        git(directory, "mv", "shared/gcanvas/renderer.cpp", "docs/renderer.cpp")
        git(directory, "commit", "-qm", "move graphics source")
        newest = git(directory, "rev-parse", "HEAD")
        assert check(directory, synchronize(newer, newest), ["shared/gcanvas/*"])

    print("CI PR-update scope: all targeted and fail-open cases passed")


if __name__ == "__main__":
    main()

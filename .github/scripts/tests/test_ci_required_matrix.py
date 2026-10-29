#!/usr/bin/env python3
"""Offline, deterministic tests for trusted module ownership / CI policy."""
import importlib.util
import tempfile
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]
MODULE = ROOT / ".github/scripts/ci_required_matrix.py"
spec = importlib.util.spec_from_file_location("ci_required_matrix", MODULE)
policy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(policy)


def matrix(*changed):
    return policy.load_workflow_requirements(list(changed), ROOT)


class RequirementTests(unittest.TestCase):
    def test_documentation_only_avoids_native_matrix(self):
        result = matrix("docs/CI_SCOPE.md", "README.md")
        self.assertEqual(result, {policy.PUBLIC: [policy.PARSER]})

    def test_docs_are_explicitly_scoped_not_any_markdown_suffix(self):
        for path in ("README.md", ".github/CI_SCOPE.md", ".github/CCACHE.md",
                     "docs/architecture.md", "documentation/guide.pdf"):
            self.assertTrue(policy.is_doc(path), path)
        for path in ("shared/markdown/tests/fixtures/table.md",
                     "flexUI/tests/fixtures/cjk.md",
                     "charts/mermaid/flowchart/examples/case.md",
                     "docs/scripts/build.py", "../docs/README.md"):
            self.assertFalse(policy.is_doc(path), path)

    def test_markdown_source_fixture_requires_native_runtime(self):
        result = matrix("shared/markdown/tests/fixtures/parser.md")
        self.assertIn(policy.RUNTIME, result)
        self.assertIn(".github/workflows/ui-core-c11.yml", result)
        self.assertIn(".github/workflows/flexui-tango-headless.yml", result)

    def test_doc_only_nul_scope_classifier_is_fail_closed(self):
        with tempfile.TemporaryDirectory(prefix="stun-doc-scope-") as temp:
            fixture = Path(temp) / "changed-paths.z"
            fixture.write_bytes(b".github/CI_SCOPE.md\\x00docs/intro.md\\x00".replace(b"\\x00", b"\x00"))
            self.assertTrue(policy.only_documentation_paths_z(fixture))
            for data in (
                b"", b"README.md", b"docs/guide.md\\x00shared/markdown/tests/fixtures/one.md\\x00".replace(b"\\x00", b"\x00"),
                b"docs/README.md\\x00docs/tests/run.py\\x00".replace(b"\\x00", b"\x00"),
                b"docs/README.md\\x00docs/../modules/danger.md\\x00".replace(b"\\x00", b"\x00"),
            ):
                fixture.write_bytes(data)
                self.assertFalse(policy.only_documentation_paths_z(fixture))
            self.assertFalse(policy.only_documentation_paths_z(Path(temp) / "missing"))

    def test_gcanvas_selects_all_real_native_targets(self):
        result = matrix("shared/gcanvas/src/platform/android/egl_host.cpp")
        self.assertIn(policy.RUNTIME, result)
        for workflow in (
            "gcanvas-android-egl.yml", "gcanvas-android-runtime.yml",
            "gcanvas-gles-runtime.yml", "gcanvas-package-contract.yml",
            "gcanvas-shader-tools-cache.yml",
        ):
            self.assertIn(".github/workflows/" + workflow, result)
        self.assertIn("android-default-cmake",
                      result[".github/workflows/gcanvas-package-contract.yml"])

    def test_markdown_selects_sdk_and_headless(self):
        result = matrix("shared/markdown/src/markdown.c")
        self.assertIn(".github/workflows/ui-core-c11.yml", result)
        self.assertIn(".github/workflows/flexui-tango-headless.yml", result)
        self.assertIn(policy.RUNTIME, result)

    def test_tango_requires_three_native_hosts_and_headless(self):
        result = matrix("tango/src/tango.cpp")
        self.assertIn(".github/workflows/tango-core.yml", result)
        self.assertEqual(len(result[".github/workflows/tango-core.yml"]), 3)
        self.assertIn(".github/workflows/flexui-tango-headless.yml", result)

    def test_graphlayout_requires_four_platform_matrix(self):
        result = matrix("shared/layout/src/layout.cpp")
        self.assertEqual(len(result[".github/workflows/graphlayout-native.yml"]), 4)
        self.assertIn(policy.RUNTIME, result)

    def test_highlighter_requires_native_and_tsan(self):
        result = matrix("flexUI/src/syntax_highlighter.cpp")
        self.assertEqual(len(result[".github/workflows/markdown-highlighter-loader.yml"]), 4)

    def test_ci_governance_source_is_native_runtime_scoped(self):
        result = matrix(".github/scripts/verify_exact_head_ci.py",
                        ".github/scripts/ci_required_matrix.py")
        self.assertIn(policy.PUBLIC, result)
        self.assertIn(policy.RUNTIME, result)

    def test_shared_ccache_action_selects_real_consumers(self):
        result = matrix(".github/actions/setup-ccache/action.yml")
        for filename in (
            "gcanvas-gles-runtime.yml", "gcanvas-package-contract.yml",
            "graphlayout-native.yml", "ui-core-c11.yml", "tango-core.yml",
            "flexui-tango-headless.yml", "markdown-highlighter-loader.yml",
        ):
            self.assertIn(".github/workflows/" + filename, result)

    def test_unknown_sources_fail_closed_even_with_docs(self):
        for paths in (
            ["new-module/src/x.c"],
            ["README.md", "unknown/bin/test"],
            ["foo.json"],
        ):
            with self.subTest(paths=paths), self.assertRaises(ValueError):
                policy.workflow_requirements(paths, {})

    def test_invalid_paths_fail_closed(self):
        for path in ("", "/absolute.c", "../x.c", "x\\y.c", "a/\x00b.c"):
            with self.subTest(path=path), self.assertRaises(ValueError):
                policy.workflow_requirements([path], {})

    def test_missing_workflow_and_negative_route_fail_closed(self):
        with self.assertRaises(ValueError):
            policy.workflow_requirements(["shared/gcanvas/x.c"], {})
        with self.assertRaises(ValueError):
            policy.pr_path_filters("on:\n  pull_request:\n    paths:\n      - '!**'\n")
        with self.assertRaises(ValueError):
            policy.pr_path_filters("on:\n  pull_request:\n    paths:\n      - '*'\n")
        with self.assertRaises(ValueError):
            policy.pr_path_filters("on:\n  pull_request:\n")

    def test_exact_trigger_contracts_are_present_on_default_branch(self):
        directory = ROOT / ".github/workflows"
        for filename in policy.WORKFLOWS:
            source = (directory / filename).read_text(encoding="utf-8")
            self.assertTrue(policy.pr_path_filters(source), filename)

    def test_pr_file_pagination_and_rename_ownership(self):
        calls = []
        def get(path):
            calls.append(path)
            return [
                {"filename": "docs/new.md", "status": "renamed",
                 "previous_filename": "shared/markdown/src/old.c"},
                {"filename": "shared/gcanvas/src/new.c", "status": "modified"},
            ]
        # One page of PR file objects is read through M0's common paging code.
        fetch_pages = lambda api, path, member: api(path + "?per_page=100&page=1")
        result = policy.pr_changed_paths(get, 37, fetch_pages)
        self.assertEqual(len(result), 3)
        self.assertIn("shared/markdown/src/old.c", result)
        self.assertEqual(calls, ["/pulls/37/files?per_page=100&page=1"])
        with self.assertRaises(ValueError):
            policy.pr_changed_paths(
                lambda _: [{"filename": "new", "status": "renamed"}], 37,
                fetch_pages
            )

    def test_all_accepted_routes_are_anchored_to_real_workflows(self):
        result = matrix("charts/mermaid/flowchart/src/flowchart_parser.y")
        self.assertIn(".github/workflows/graphlayout-native.yml", result)
        self.assertIn(policy.RUNTIME, result)
        self.assertIn(policy.PUBLIC, result)


if __name__ == "__main__":
    unittest.main(verbosity=1)

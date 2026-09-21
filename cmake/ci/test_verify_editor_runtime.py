"""Unit tests for evidence parsing; these do not execute or replace the JIT."""
import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import verify_editor_runtime as audit


class EditorEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.evidence = self.root / 'evidence'
        self.evidence.mkdir()
        self.sdk = self.root / 'sdk'
        self.tbs = self.root / 'tbs'
        self.app = self.root / 'app'
        self.original = self.root / 'build/editor'
        self.archive = self.root / 'editor-consumer-quarantine'
        self.mesa = self.root / 'mesa'
        self.env = {'EVIDENCE_DIR': str(self.evidence), 'SALTS_ROOT': str(self.sdk),
                    'TURBOSCRIPT_ROOT': str(self.tbs), 'APP_ROOT': str(self.app),
                    'RUNNER_TEMP': str(self.root), 'GITHUB_SHA': 'workflow-commit'}
        self.libs = [self.sdk / 'lib/libsalts.so.1', self.sdk / 'lib/libdata_bind.so',
                     self.tbs / 'lib/libturbo_script.so']
        self.graphics = [self.mesa / 'libGLX_mesa.so.0.0.0', self.mesa / 'libgallium-25.2.8.so']
        self.plugin = self.archive / 'bin/libflexui_document_service.so'
        self.installed_plugin = self.app / 'bin/libflexui_document_service.so'
        for p in self.libs + self.graphics + [self.plugin, self.installed_plugin]:
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(p.name.encode())
        (self.evidence / 'build-document-service.sha256').write_text(hashlib.sha256(self.plugin.read_bytes()).hexdigest() + '  build/editor/bin/libflexui_document_service.so\n')
        self.tests = ['test_turboscript_controller', 'test_turboscript_application',
                      'test_desktop_editor_controller', 'test_flexui_desktop_editor_smoke']
        (self.evidence / 'selected-tests.json').write_text(json.dumps({'tests': [{'name': n} for n in self.tests]}))
        (self.evidence / 'editor-results.xml').write_text('<testsuite>' + ''.join(f'<testcase name="{n}"/>' for n in self.tests) + '</testsuite>')
        summary = '\n'.join(f'Total tests [{s}]: {1 if s == "PASSED" else 0}' for s in ('PASSED', 'FAILED', 'SKIPPED', 'FILTERED', 'TODO'))
        (self.evidence / 'editor-ctest.log').write_text((summary + '\nAssertions: 1 passed, 0 failed\n') * 3 + 'FlexUI desktop editor smoke test passed: example\n')
        (self.evidence / 'installed-smoke.log').write_text('FlexUI desktop editor smoke test passed: example\n')
        (self.evidence / 'mesa-providers.json').write_text(json.dumps({str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in self.graphics}))
        self.programs = [self.original / f'bin/{n}' for n in self.tests[:3]] + [self.original / 'bin/flexui_desktop_editor', self.app / 'bin/flexui_desktop_editor']
        for i, program in enumerate(self.programs):
            paths = list(self.libs)
            if program.name == 'flexui_desktop_editor':
                paths += self.graphics + [self.original / 'bin/libflexui_document_service.so' if i == 3 else self.installed_plugin]
            (self.evidence / f'loader.{i}').write_text(f'1: transferring control: {program}\n' + ''.join(f'1: calling init: {p}\n' for p in paths))

    def execute(self):
        def command(args, **kwargs):
            if args[:2] == ['git', 'rev-parse']:
                return 'source-head\n'
            if args[0] in ('readelf', 'ldd'):
                return 'fixture: installed runtime dependencies\n'
            raise AssertionError(args)
        with patch.dict(os.environ, self.env), patch.object(Path, 'cwd', return_value=self.root), patch.object(audit.subprocess, 'check_output', side_effect=command), contextlib.redirect_stdout(io.StringIO()):
            audit.main()

    def test_valid_installed_profile_and_quarantined_document_plugin(self):
        self.execute()
        result = json.loads((self.evidence / 'editor-evidence.json').read_text())
        self.assertEqual(result['source_head'], 'source-head')
        self.assertEqual(result['workflow_commit'], 'workflow-commit')
        self.assertEqual(len(result['processes']), 5)

    def test_modified_quarantined_plugin_is_rejected(self):
        self.plugin.write_bytes(b'changed')
        with self.assertRaises(AssertionError): self.execute()

    def test_arbitrary_missing_library_cannot_use_quarantine(self):
        with (self.evidence / 'loader.3').open('a') as f:
            f.write(f'1: calling init: {self.original}/bin/libunknown.so\n')
        with self.assertRaises(FileNotFoundError): self.execute()

    def test_missing_process_is_rejected(self):
        (self.evidence / 'loader.4').unlink()
        with self.assertRaises(AssertionError): self.execute()

    def test_wrong_core_provider_is_rejected(self):
        rogue = self.root / 'rogue/libsalts.so.1'
        rogue.parent.mkdir(); rogue.write_bytes(b'wrong-core')
        trace = self.evidence / 'loader.1'
        trace.write_text(trace.read_text().replace(str(self.libs[0]), str(rogue)))
        with self.assertRaises(AssertionError): self.execute()

    def test_failed_ctest_is_rejected(self):
        path = self.evidence / 'editor-results.xml'
        path.write_text(path.read_text().replace(f'<testcase name="{self.tests[0]}"/>', f'<testcase name="{self.tests[0]}"><failure/></testcase>'))
        with self.assertRaises(AssertionError): self.execute()

    def test_installed_editor_cannot_load_build_tree_plugin(self):
        trace = self.evidence / 'loader.4'
        trace.write_text(trace.read_text().replace(str(self.installed_plugin),
                                                 str(self.original / 'bin/libflexui_document_service.so')))
        with self.assertRaises(AssertionError): self.execute()

    def test_graphics_hash_mismatch_is_rejected(self):
        self.graphics[0].write_bytes(b'wrong-driver')
        with self.assertRaises(AssertionError): self.execute()

    def test_missing_complete_tinytest_summary_is_rejected(self):
        path = self.evidence / 'editor-ctest.log'
        path.write_text(path.read_text().replace('Total tests [PASSED]: 1', '', 1))
        with self.assertRaises(AssertionError): self.execute()


if __name__ == '__main__':
    unittest.main()

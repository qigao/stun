"""Read-only audit of the real TurboScript editor and installed SDK evidence (#31)."""
from collections import Counter
import hashlib
import json
import os
import re
import subprocess
from pathlib import Path
import xml.etree.ElementTree as ET


def resolve_loaded_library(raw_path: str, original_build: Path, quarantined_build: Path,
                           expected_plugin_hash: str) -> Path:
    """Resolve a trace path without accepting an arbitrary missing producer library."""
    path = Path(raw_path)
    original_plugin = original_build / 'bin/libflexui_document_service.so'
    if path == original_plugin:
        archived = (quarantined_build / 'bin/libflexui_document_service.so').resolve(strict=True)
        assert hashlib.sha256(archived.read_bytes()).hexdigest() == expected_plugin_hash
        return archived
    return path.resolve(strict=True)


def main() -> None:
    e = Path(os.environ['EVIDENCE_DIR'])
    expected = {'test_turboscript_controller', 'test_turboscript_application',
                'test_desktop_editor_controller', 'test_flexui_desktop_editor_smoke'}
    selected = json.loads((e / 'selected-tests.json').read_text())['tests']
    cases = list(ET.parse(e / 'editor-results.xml').getroot().iter('testcase'))
    assert len(selected) == 4 and {t['name'] for t in selected} == expected
    assert len(cases) == 4 and {t.attrib['name'] for t in cases} == expected
    assert not any(list(c.iter(tag)) for c in cases for tag in ('failure', 'error', 'skipped'))
    log = re.sub(r'\x1b\[[0-?]*[ -/]*[@-~]', '', (e / 'editor-ctest.log').read_text())
    installed_log = (e / 'installed-smoke.log').read_text()
    assert not re.search(r'ERROR: (AddressSanitizer|LeakSanitizer)|runtime error:|VULKAN VALIDATION \[(ERROR|WARNING)\]', log + installed_log)
    assert 'FlexUI desktop editor smoke test passed:' in log and 'FlexUI desktop editor smoke test passed:' in installed_log
    totals = {}
    for status in ('PASSED', 'FAILED', 'SKIPPED', 'FILTERED', 'TODO'):
        values = [int(n) for n in re.findall(rf'Total tests \[{status}\]:\s*(\d+)', log)]
        assert len(values) == 3 and all(n > 0 if status == 'PASSED' else n == 0 for n in values), (status, values)
        totals[status] = sum(values)
    assertions = re.findall(r'Assertions:\s*(\d+) passed,\s*(\d+) failed', log)
    assert len(assertions) == 3 and all(int(f) == 0 for _, f in assertions)
    totals['assertions'] = sum(int(p) for p, _ in assertions)
    sdk = Path(os.environ['SALTS_ROOT']).resolve(strict=True)
    tbs = Path(os.environ['TURBOSCRIPT_ROOT']).resolve(strict=True)
    mesa = {Path(p): h for p, h in json.loads((e / 'mesa-providers.json').read_text()).items()}
    for p, h in mesa.items():
        assert hashlib.sha256(p.read_bytes()).hexdigest() == h
    expected_programs = Counter({'test_turboscript_controller': 1, 'test_turboscript_application': 1,
                                 'test_desktop_editor_controller': 1, 'flexui_desktop_editor': 2})
    observed = Counter()
    records = []
    for trace in sorted(e.glob('loader.*')):
        text = trace.read_text()
        programs = re.findall(r'transferring control:[ \t]+(\S+)', text)
        if len(programs) != 1 or Path(programs[0]).name not in expected_programs:
            continue
        name = Path(programs[0]).name
        paths = set(re.findall(r'calling init:[ \t]+([^\s\[]+)', text))
        # Only the document plugin moved with the build tree. Its saved hash binds
        # the old trace path to that exact image, not to an arbitrary replacement.
        libs = {resolve_loaded_library(p, Path.cwd() / 'build/editor',
                                       Path(os.environ['RUNNER_TEMP']) / 'editor-consumer-quarantine',
                                       (e / 'build-document-service.sha256').read_text().split()[0])
                for p in paths}
        core = {p for p in libs if re.fullmatch(r'libsalts(?:[_-]core)?\.so(?:\.\d+)*', p.name, re.I)}
        runtime = {p for p in libs if p.name.startswith('libturbo_script.so')}
        databind = {p for p in libs if p.name.startswith('libdata_bind.so')}
        assert len(core) == len(runtime) == len(databind) == 1, (name, core, runtime, databind)
        assert all(p.is_relative_to(sdk) for p in core | databind)
        assert all(p.is_relative_to(sdk) for p in libs if p.name.startswith('libsalts'))
        assert all(p.is_relative_to(tbs) for p in runtime)
        graphics = {p for p in libs if p.name.startswith(('libgallium', 'libGLX_mesa'))}
        assert graphics == (set(mesa) if name == 'flexui_desktop_editor' else set()), (name, graphics)
        plugins = {p for p in libs if p.name == 'libflexui_document_service.so'}
        expected_plugin = set()
        if name == 'flexui_desktop_editor':
            installed_editor = Path(os.environ['APP_ROOT']) / 'bin/flexui_desktop_editor'
            plugin_root = (Path(os.environ['APP_ROOT']) if programs[0] == str(installed_editor)
                           else Path(os.environ['RUNNER_TEMP']) / 'editor-consumer-quarantine')
            expected_plugin = {(plugin_root / 'bin/libflexui_document_service.so').resolve(strict=True)}
        assert plugins == expected_plugin, (name, plugins)
        assert not any(re.search(r'libturbo[_-]?(utils|parser|net)', p.name, re.I) for p in libs)
        observed[name] += 1
        records.append({'program': programs[0], 'core': list(map(str, core)),
                        'turboscript': list(map(str, runtime)), 'databind': list(map(str, databind)),
                        'graphics': list(map(str, graphics))})
    assert observed == expected_programs, observed
    installed = Path(os.environ['APP_ROOT']) / 'bin/flexui_desktop_editor'
    assert sum(r['program'] == str(installed) for r in records) == 1
    dynamic = subprocess.check_output(['readelf', '-d', str(installed)], text=True)
    loader = subprocess.check_output(['ldd', str(installed)], text=True)
    (e / 'installed-editor-dynamic.txt').write_text(dynamic + '\n' + loader)
    assert 'not found' not in loader
    forbidden = ('.ci/sources', 'stun-editor-producers', 'editor-source-quarantine', 'editor-build-quarantine', 'editor-consumer-quarantine')
    assert not any(p in dynamic + loader for p in forbidden)
    receipt = {'source_head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
               'workflow_commit': os.environ['GITHUB_SHA'], 'ctest': 4, 'installed_smoke': 1,
               'tinytest': totals, 'processes': records,
               'scope': 'Linux OpenGL software-rendered real JIT editor; explicit installed SDKs, not a relocatable standalone distribution or Windows/macOS claim.'}
    (e / 'editor-evidence.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()

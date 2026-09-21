"""Validate complete Host ABI CTest receipts; this never runs a build or runtime."""
from collections import Counter
import json
import os
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET


def verify(root: Path, kind: str) -> dict:
    if kind == 'internal':
        names = {f'test_turbo_script_host_{suite}' for suite in
                 ('result', 'registry', 'module', 'instance', 'call', 'state', 'limits')}
    elif kind == 'installed':
        names = {f'installed_host_{language}_{mode}' for language in ('c', 'cpp')
                 for mode in ('interpreter', 'jit')}
    else:
        raise ValueError('Expected internal or installed Host ABI receipts')
    prefix = f'host-{kind}'
    selected = json.loads((root / f'{prefix}-selected.json').read_text())['tests']
    cases = list(ET.parse(root / f'{prefix}-results.xml').getroot().iter('testcase'))
    assert len(selected) == len(names) and {t['name'] for t in selected} == names
    assert len(cases) == len(names) and {c.attrib['name'] for c in cases} == names
    assert all(c.attrib.get('status', 'run') == 'run' for c in cases)
    assert not any(list(c.iter(tag)) for c in cases for tag in ('failure', 'error', 'skipped'))
    log = re.sub(r'\x1b\[[0-?]*[ -/]*[@-~]', '', (root / f'{prefix}.log').read_text())
    assert not re.search(r'ERROR: (AddressSanitizer|LeakSanitizer)|AddressSanitizer:DEADLYSIGNAL|runtime error:', log)
    receipt = {'kind': kind, 'ctest_cases': sorted(names)}
    if kind == 'internal':
        totals = {}
        for status in ('PASSED', 'FAILED', 'SKIPPED', 'FILTERED', 'TODO'):
            values = [int(n) for n in re.findall(rf'Total tests \[{status}\]:\s*(\d+)', log)]
            assert len(values) == len(names), (status, values)
            assert all(n > 0 if status == 'PASSED' else n == 0 for n in values), (status, values)
            totals[status] = sum(values)
        assertions = re.findall(r'Assertions:\s*(\d+) passed,\s*(\d+) failed', log)
        assert len(assertions) == len(names) and all(int(p) > 0 and int(f) == 0 for p, f in assertions)
        totals['assertions'] = sum(int(p) for p, _ in assertions)
        receipt['tinytest'] = totals
    else:
        modes = re.findall(r'Installed Host ABI v1 passed: mode=(\d+);', log)
        assert Counter(modes) == Counter({'1': 2, '2': 2}), modes
    return receipt


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('usage: verify_host_abi_tests.py internal|installed')
    evidence = Path(os.environ['EVIDENCE_DIR'])
    result = verify(evidence, sys.argv[1])
    payload = json.dumps(result, indent=2) + '\n'
    (evidence / f'host-{sys.argv[1]}-receipt.json').write_text(payload)
    print(payload)

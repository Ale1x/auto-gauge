#!/usr/bin/env python3
"""Document integrity and arithmetic only. Does not validate hardware."""
import csv
import hashlib
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
trace = (root/'traceability.md').read_text()
assert re.findall(r'^\| (PR-\d{3}) \|', trace, re.M) == [f'PR-{i:03d}' for i in range(1,27)]
assert re.findall(r'^\| (O-\d{2}) \|', trace, re.M) == [f'O-{i:02d}' for i in range(1,26)]
with (root/'BOM_CANDIDATES.csv').open(newline='') as f:
    rows = list(csv.DictReader(f))
assert len(rows) == 19
assert all(None not in r for r in rows)
assert all(r['stato'] for r in rows)
assert round(1.75*25.4,2) == 44.45
assert round(48.96-44.45,2) == 4.51
assert round(46.35-44.45,2) == 1.90
assert round(48.96 + 2*(1.0+0.3), 2) == 51.56
assert round((52-48.96)/2-1.0-0.3, 2) == 0.22
assert 'DEC-002' in trace
assert 'v0.2.2' in trace
assert 466*466*2 == 434312
assert 15*5*16*3600 == 4320000
assert 8*(1-1/16) == 7.5
assert round(100e-6*24*28,4) == 0.0672
for name, info in json.loads((root/'evidence/manifest.json').read_text())['files'].items():
    assert hashlib.sha256((root/'evidence'/name).read_bytes()).hexdigest() == info['sha256']
for name in ['DECISIONS.md','MECHANICAL_K0.md','CAN_POWER_TEST_PLAN.md','SUPPLIER_RFQ_DRAFT.md','GATE_K0.md','issues/K0-001_BLOCKERS.md','evidence/SOURCES.md']:
    assert (root/name).is_file()
assert not list(root.glob('*.kicad_sch'))
assert not list(root.glob('*.kicad_pcb'))
print('PASS: 26 PR, 25 O, 19 BOM rows, arithmetic and PDF hashes. Hardware validation: NOT EXECUTED.')

#!/usr/bin/env python3
"""Document integrity and arithmetic only. Does not validate hardware."""
import csv
import hashlib
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
trace = (root/'docs/project/traceability.md').read_text()
assert re.findall(r'^\| (PR-\d{3}) \|', trace, re.M) == [f'PR-{i:03d}' for i in range(1,27)]
assert re.findall(r'^\| (O-\d{2}) \|', trace, re.M) == [f'O-{i:02d}' for i in range(1,26)]
with (root/'hardware/BOM_CANDIDATES.csv').open(newline='') as f:
    rows = list(csv.DictReader(f))
assert len(rows) == 25
assert all(None not in r for r in rows)
assert all(r['stato'] for r in rows)
assert round(1.75*25.4,2) == 44.45
assert round(48.96-44.45,2) == 4.51
assert round(46.35-44.45,2) == 1.90
assert round(48.96 + 2*(1.0+0.3), 2) == 51.56
assert round((52-48.96)/2-1.0-0.3, 2) == 0.22
assert 'DEC-002' in trace
assert 'v0.2.8' in trace
assert 'DEC-011' in trace and 'una sola connessione attiva' in trace
assert 'DEC-012' in trace and 'CloudKit' in trace
assert 'DEC-014' in trace and 'codice' in trace
assert 'DEC-008' in trace
assert 'DEC-003' in trace and 'DEC-004' in trace and 'DEC-005' in trace
assert 'DEC-006' in trace and 'DEC-007' in trace
assert round(51.55 + 2*(1.0+0.3),2) == 54.15
assert 466*466*2 == 434312
assert 15*5*16*3600 == 4320000
assert 8*(1-1/16) == 7.5
assert round(2*1024**2/(5*16)/3600,2) == 7.28
assert round(2*1024**2/(5*5*16)/3600,2) == 1.46
assert 30+10+15 == 55
assert round(100e-6*24*28,4) == 0.0672
for name, info in json.loads((root/'evidence/manifest.json').read_text())['files'].items():
    assert hashlib.sha256((root/'evidence'/name).read_bytes()).hexdigest() == info['sha256']
for name in ['docs/project/DECISIONS.md','docs/hardware/MECHANICAL_K0.md','docs/hardware/CAN_POWER_TEST_PLAN.md','docs/suppliers/SUPPLIER_RFQ_DRAFT.md','docs/hardware/GATE_K0.md','docs/hardware/issues/K0-001_BLOCKERS.md','evidence/SOURCES.md','docs/hardware/LOGGING_K0.md','docs/hardware/STANDBY_BUDGET_K0.md','docs/hardware/CONNECTORS_CABLE_K0.md','docs/hardware/HW_IGN_TEST_PROTOCOL.md','tests/templates/hw_ign_samples.csv','docs/hardware/USB_RECOVERY_K0.md','docs/suppliers/SUPPLIER_CONTACT_LOG.md','docs/suppliers/SUPPLIER_MESSAGES_READY.md','docs/hardware/CAPACITY_K0.md','docs/hardware/POWER_ENVELOPE_K0.md','docs/hardware/MCU_RAIL_K0.md','evidence/models/mcu_rail_tradeoff.csv','scripts/analyze_hw_ign.py','evidence/models/power_cable_sensitivity.csv','evidence/models/flash_sensitivity.csv','evidence/models/log_sensitivity.csv','evidence/models/can_latency_sensitivity.csv']:
    assert (root/name).is_file()
assert 'DEC-009' in (root/'docs/project/DECISIONS.md').read_text()
assert (root/'hardware/auto_gauge.kicad_sch').is_file()
assert not list((root/'hardware').glob('*.kicad_pcb'))
print('PASS: 26 PR, 25 O, 25 BOM rows, arithmetic and PDF hashes. Hardware validation: NOT EXECUTED.')

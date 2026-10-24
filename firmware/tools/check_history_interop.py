#!/usr/bin/env python3
"""Compare sanitized C JSON pages with shared goldens and Python/SQLite."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'shared/reference'))
from log_import import LogArchive
from log_source import LogPageSource


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', required=True)
    args = parser.parse_args()
    cases = json.loads((ROOT / 'shared/fixtures/pagination.cases.json').read_text())
    output = subprocess.check_output([args.executable], text=True)
    if output != subprocess.check_output([args.executable], text=True):
        raise AssertionError('C serialization is not deterministic')
    lines = list(map(json.loads, output.splitlines()))
    actual = {(item['case'], item['read_index']): item['page'] for item in lines}
    assert len(actual) == len(lines), 'Duplicate harness output'
    total = 0
    for case in cases:
        source = LogPageSource(case['session_id'], case['capacity'], int(case['first_sequence']))
        for record in case['records']:
            source.append(record['pid'], record['ecu_source'], int(record['monotonic_ms']),
                          record['status'], bytes.fromhex(record['raw_hex']))
        a, b = LogArchive(':memory:'), LogArchive(':memory:')
        try:
            for index, read in enumerate(case['reads']):
                page = actual.pop((case['name'], index))
                assert page == read['page'] == source.read_page(read['after_sequence'], read['limit'])
                assert len(json.dumps(page, separators=(',', ':')).encode()) <= 4096
                ack_a = a.import_page('gauge', page)
                source.acknowledge(case['session_id'], ack_a['acknowledged_through'])
                ack_b = b.import_page('gauge', page)
                assert ack_b == ack_a
                total += 1
        finally:
            a.close()
            b.close()
    assert not actual
    print(f'PASS: {len(cases)} shared fixtures, {total} C/Python/golden pages, two independent SQLite clients, deterministic C JSON')


if __name__ == '__main__':
    main()

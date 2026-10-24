#!/usr/bin/env python3
"""Build-only shared JSON -> typed C golden pages; not a device parser."""
import json
from pathlib import Path
import sys


def uint(value):
    if type(value) is not str or not value.isascii() or not value.isdigit():
        raise ValueError('Invalid canonical uint64')
    number = int(value)
    if str(number) != value or not 0 <= number <= (1 << 64) - 1:
        raise ValueError('Invalid uint64 range')
    return f'UINT64_C({number})'


def bytes_c(value):
    raw = bytes.fromhex(value)
    return '{' + (','.join(f'0x{byte:02x}' for byte in raw) if raw else '0') + '}'


status = {name: 'AG_SAMPLE_' + name.upper() for name in
          ('valid', 'unsupported', 'unavailable', 'stale', 'timeout', 'error')}


def record_c(record):
    return ('{.sequence=' + uint(record['sequence']) + ',.monotonic_ms=' + uint(record['monotonic_ms'])
            + ',.pid=' + str(record['pid']) + ',.ecu_source=' + str(record['ecu_source'])
            + ',.status=' + status[record['status']] + ',.raw_length=' + str(len(record['raw_hex']) // 2)
            + ',.raw=' + bytes_c(record['raw_hex']) + '}')


source, destination = map(Path, sys.argv[1:])
cases = json.loads(source.read_text())
out = ['/* Generated in build directory from shared pagination.cases.json. */',
       '#include "ag_history.h"',
       'typedef struct {uint64_t cursor; size_t limit, record_count; uint64_t through; const char *json;} ag_history_read_fixture_t;',
       'typedef struct {uint64_t through; size_t record_count;} ag_history_ack_fixture_t;',
       'typedef struct {const char *name; uint8_t session[16]; size_t capacity; uint64_t first_sequence; size_t record_count; const ag_history_record_t *records; size_t read_count; const ag_history_read_fixture_t *reads; size_t ack_count; const ag_history_ack_fixture_t *acks;} ag_history_fixture_t;']
for index, case in enumerate(cases):
    rows = [record_c(record) for record in case['records']] or ['{0}']
    out += [f'static const ag_history_record_t kHistoryRecords{index}[]={{' + ',\n'.join(rows) + '};']
    reads = []
    for read in case['reads']:
        page = read['page']
        # Normalize only object ordering; values remain those of the shared golden.
        canonical = dict(session_id=page['session_id'], records=[
            {key: record[key] for key in ('sequence', 'monotonic_ms', 'pid', 'ecu_source', 'status', 'raw_hex')}
            for record in page['records']], lost=[{key: interval[key] for key in ('first', 'last')} for interval in page['lost']])
        through = (page['records'][-1]['sequence'] if page['records'] else
                   page['lost'][-1]['last'] if page['lost'] else read['after_sequence'])
        reads.append('{.cursor=' + uint(read['after_sequence']) + ',.limit=' + str(read['limit'])
                     + ',.record_count=' + str(len(page['records'])) + ',.through=' + uint(through)
                     + ',.json=' + json.dumps(json.dumps(canonical, separators=(',', ':'))) + '}')
    out += [f'static const ag_history_read_fixture_t kHistoryReads{index}[]={{' + ',\n'.join(reads) + '};']
    acks = ['{.through=' + uint(ack['through']) + ',.record_count=' + str(ack['record_count']) + '}' for ack in case['acknowledgements']]
    out += [f'static const ag_history_ack_fixture_t kHistoryAcks{index}[]={{' + ',\n'.join(acks or ['{0}']) + '};']
rows = []
for index, case in enumerate(cases):
    rows.append('{.name=' + json.dumps(case['name']) + ',.session=' + bytes_c(case['session_id'])
                + ',.capacity=' + str(case['capacity']) + ',.first_sequence=' + uint(case['first_sequence'])
                + ',.record_count=' + str(len(case['records'])) + f',.records=kHistoryRecords{index}'
                + ',.read_count=' + str(len(case['reads'])) + f',.reads=kHistoryReads{index}'
                + ',.ack_count=' + str(len(case['acknowledgements'])) + f',.acks=kHistoryAcks{index}' + '}')
out += ['static const ag_history_fixture_t kHistoryFixtures[]={\n' + ',\n'.join(rows) + '\n};',
        'static const size_t kHistoryFixtureCount=sizeof(kHistoryFixtures)/sizeof(kHistoryFixtures[0]);']
destination.write_text('\n'.join(out) + '\n')
print(f'Generated {len(cases)} shared pagination fixtures')

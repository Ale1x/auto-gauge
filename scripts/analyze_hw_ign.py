#!/usr/bin/env python3
"""Summarize real HW-IGN captures. This tool never issues a compliance PASS."""
import argparse
import csv
import json
import math
from pathlib import Path

FIELDS = ['timestamp_monotonic_s', 'dut_input_v', 'dut_current_a',
          'ign_ground_truth_ON_OFF_UNKNOWN', 'dut_state',
          'dut_can_tx_cumulative', 'dut_obd_requests_cumulative',
          'display_on', 'event_note']

def analyze(path):
    with Path(path).open(newline='') as f:
        reader = csv.DictReader(f)
        if reader.fieldnames != FIELDS:
            raise ValueError('CSV header does not match the HW-IGN template')
        samples = list(reader)
    report = dict(verdict='NOT_DETERMINED', sample_count=len(samples),
                  limitation='Sample observations only; bandwidth, uncertainty, setup and gaps require review. No hardware compliance verdict.')
    if not samples:
        report['capture_status'] = 'NO_DATA'
        return report
    previous = None
    off_start = None
    delays = []
    currents, voltages = [], []
    late_off = []
    counter_events = []
    known_off_transitions = 0
    for line, row in enumerate(samples, 2):
        if None in row or any(v is None for v in row.values()):
            raise ValueError(f'line {line}: malformed CSV row')
        values = {}
        for name in FIELDS[:3]:
            try:
                value = float(row[name])
            except ValueError as exc:
                raise ValueError(f'line {line}: invalid {name}') from exc
            if not math.isfinite(value):
                raise ValueError(f'line {line}: non-finite {name}')
            values[name] = value
        now = values[FIELDS[0]]
        truth = row[FIELDS[3]]
        if truth not in ('ON', 'OFF', 'UNKNOWN'):
            raise ValueError(f'line {line}: invalid IGN ground truth')
        if row['display_on'] not in ('0', '1', 'UNKNOWN'):
            raise ValueError(f'line {line}: display_on must be 0, 1 or UNKNOWN')
        counters = {}
        for name in FIELDS[5:7]:
            try:
                counters[name] = int(row[name])
            except ValueError as exc:
                raise ValueError(f'line {line}: invalid {name}') from exc
            if counters[name] < 0:
                raise ValueError(f'line {line}: negative counter')
        if previous:
            if now <= previous['time']:
                raise ValueError(f'line {line}: timestamps must strictly increase')
            delays.append(now-previous['time'])
            if any(counters[k] < previous['counters'][k] for k in counters):
                raise ValueError(f'line {line}: counter reset/wrap; split capture into runs')
        if truth != 'OFF':
            off_start = None
        elif previous and previous['truth'] == 'ON':
            off_start = now
            known_off_transitions += 1
        # A run starting OFF or an UNKNOWN→OFF transition cannot prove the
        # actual OFF time, so no 300-second timing claim is derived from it.
        if truth == 'OFF' and off_start is not None and now-off_start >= 300:
            late_off.append(dict(line=line, timestamp_s=now,
                                 input_v=values['dut_input_v'],
                                 current_a=values['dut_current_a'],
                                 above_100uA=values['dut_current_a'] > 100e-6))
            if previous and previous['time'] >= off_start+300:
                delta = counters['dut_obd_requests_cumulative']-previous['counters']['dut_obd_requests_cumulative']
                if delta:
                    counter_events.append(dict(interval_start_s=previous['time'],
                                               interval_end_s=now, obd_requests_delta=delta))
        currents.append(values['dut_current_a'])
        voltages.append(values['dut_input_v'])
        previous = dict(time=now, truth=truth, counters=counters)
    report.update(capture_status='OBSERVATIONS_AVAILABLE',
                  duration_s=float(samples[-1][FIELDS[0]])-float(samples[0][FIELDS[0]]),
                  max_sample_gap_s=max(delays) if delays else None,
                  input_voltage_range_v=[min(voltages), max(voltages)],
                  sampled_current_range_a=[min(currents), max(currents)],
                  observed_ON_to_OFF_transitions=known_off_transitions,
                  samples_after_observed_OFF_plus_300s=late_off,
                  obd_counter_changes_in_late_OFF_intervals=counter_events)
    return report

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    args = parser.parse_args()
    try:
        result = analyze(args.capture)
    except (ValueError, OSError) as exc:
        parser.exit(2, f'Invalid capture: {exc}\n')
    print(json.dumps(result, indent=2, allow_nan=False))

if __name__ == '__main__':
    main()

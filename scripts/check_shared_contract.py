#!/usr/bin/env python3
"""Validate shared development fixtures; this does not authorize config apply."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def integer(value, low, high):
    return type(value) is int and low <= value <= high


def validate(config):
    if not isinstance(config, dict) or config.get('schema_version') != 1 or type(config.get('schema_version')) is not int:
        return False
    if not integer(config.get('revision'), 1, 2147483647):
        return False
    brightness = config.get('brightness')
    if not isinstance(brightness, dict) or brightness.get('mode') not in ('manual', 'auto') or not integer(brightness.get('manual_percent'), 0, 100):
        return False
    dashboards = config.get('dashboards')
    if not isinstance(dashboards, list) or not 1 <= len(dashboards) <= 3:
        return False
    dashboard_ids = set()
    for dashboard in dashboards:
        if not isinstance(dashboard, dict) or not integer(dashboard.get('id'), 1, 255) or dashboard['id'] in dashboard_ids:
            return False
        dashboard_ids.add(dashboard['id'])
        widgets = dashboard.get('widgets')
        if not isinstance(widgets, list) or not 1 <= len(widgets) <= 5:
            return False
        widget_ids = set()
        for widget in widgets:
            if not isinstance(widget, dict) or not integer(widget.get('id'), 1, 255) or widget['id'] in widget_ids:
                return False
            widget_ids.add(widget['id'])
            if not integer(widget.get('pid'), 1, 255):
                return False
            rect = widget.get('rect')
            if not isinstance(rect, dict):
                return False
            if not all(integer(rect.get(k), 0 if k in ('x', 'y') else 1, 10000) for k in ('x', 'y', 'width', 'height')):
                return False
            if rect['x'] + rect['width'] > 10000 or rect['y'] + rect['height'] > 10000:
                return False
    return True


def main():
    cases = json.loads((ROOT / 'shared/fixtures/config.cases.json').read_text())
    for case in cases:
        actual = validate(case['configuration'])
        if actual != case['valid']:
            raise SystemExit(f"Contract mismatch: {case['name']}, expected {case['valid']}, got {actual}")
    if not validate(json.loads((ROOT / 'shared/fixtures/config.valid.json').read_text())):
        raise SystemExit('Shared sample invalid')
    print(f'PASS: {len(cases)} shared configuration cases; structural validation only, no device authorization.')


if __name__ == '__main__':
    main()

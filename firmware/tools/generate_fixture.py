#!/usr/bin/env python3
"""Build-only JSON -> C fixtures. This is NOT a device JSON decoder."""
import json
from pathlib import Path
import sys


def integer(value):
    if type(value) is not int:
        raise ValueError('JSON booleans/fractions cannot be represented by a typed C integer fixture')
    return str(value)


def config(value):
    dashboards = []
    for dashboard in value['dashboards'][:3]:
        widgets = []
        for widget in dashboard['widgets'][:5]:
            rect = widget['rect']
            widgets.append('{.id=' + integer(widget['id']) + ',.pid=' + integer(widget['pid'])
                           + ',.rect={' + ','.join('.' + key + '=' + integer(rect[key])
                                                  for key in ('x', 'y', 'width', 'height')) + '}}')
        dashboards.append('{.id=' + integer(dashboard['id'])
                          + ',.widget_count=' + str(len(dashboard['widgets']))
                          + (',.widgets={' + ','.join(widgets) + '}' if widgets else '') + '}')
    mode = {'manual': 0, 'auto': 1}.get(value['brightness']['mode'], 255)
    return ('{.schema_version=' + integer(value['schema_version'])
            + ',.revision=' + integer(value['revision'])
            + ',.brightness_mode=' + str(mode)
            + ',.manual_percent=' + integer(value['brightness']['manual_percent'])
            + ',.dashboard_count=' + str(len(value['dashboards']))
            + (',.dashboards={' + ','.join(dashboards) + '}' if dashboards else '') + '}')


sample, cases, output = map(Path, sys.argv[1:])
rows, skipped = [], []
for case in json.loads(cases.read_text()):
    try:
        initialization = config(case['configuration'])
    except ValueError:
        skipped.append(case['name'])
        continue
    rows.append('{.name=' + json.dumps(case['name']) + ',.valid='
                + ('true' if case['valid'] else 'false') + ',.configuration=' + initialization + '}')
output.write_text('/* Generated from shared JSON; do not edit. */\n'
                  '#include "ag_core.h"\n'
                  'static const ag_config_t kSharedFixture=' + config(json.loads(sample.read_text())) + ';\n'
                  'typedef struct {const char *name; bool valid; ag_config_t configuration;} ag_fixture_case_t;\n'
                  'static const ag_fixture_case_t kSharedCases[]={\n' + ',\n'.join(rows) + '\n};\n'
                  '/* Cases requiring a JSON parser: ' + ', '.join(skipped) + ' */\n')
print(f'Generated {len(rows)} typed C contract cases; {len(skipped)} JSON-type cases reserved for future parser')

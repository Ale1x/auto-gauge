#!/usr/bin/env python3
"""Build-only JSON -> C fixtures. This is NOT a device JSON decoder."""
import binascii
import json
import struct
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
golden_config = json.loads(sample.read_text())
golden_payload = struct.pack('<IIBBB', golden_config['schema_version'], golden_config['revision'],
                             {'manual': 0, 'auto': 1}[golden_config['brightness']['mode']],
                             golden_config['brightness']['manual_percent'], len(golden_config['dashboards']))
for dashboard in golden_config['dashboards']:
    golden_payload += struct.pack('<BB', dashboard['id'], len(dashboard['widgets']))
    for widget in dashboard['widgets']:
        golden_payload += struct.pack('<BBHHHH', widget['id'], widget['pid'],
                                     *[widget['rect'][key] for key in ('x', 'y', 'width', 'height')])
golden_header = bytearray(struct.pack('<4sHHQIIII', b'AGCF', 1, len(golden_payload), 1,
                                     golden_config['schema_version'], golden_config['revision'], 0, 0))
struct.pack_into('<I', golden_header, 24, binascii.crc32(golden_header[:24] + golden_header[28:] + golden_payload))
golden_record = golden_header + golden_payload
output.write_text('/* Generated from shared JSON; do not edit. */\n'
                  '#include "ag_core.h"\n'
                  'static const ag_config_t kSharedFixture=' + config(json.loads(sample.read_text())) + ';\n'
                  'typedef struct {const char *name; bool valid; ag_config_t configuration;} ag_fixture_case_t;\n'
                  'static const ag_fixture_case_t kSharedCases[]={\n' + ',\n'.join(rows) + '\n};\n'
                  '/* Cases requiring a JSON parser: ' + ', '.join(skipped) + ' */\n'
                  '#ifdef AG_CONFIG_STORE_FIXTURE\n'
                  'static const uint8_t kConfigRecordGolden[]={'
                  + ','.join(f'0x{byte:02x}' for byte in golden_record) + '};\n#endif\n')
print(f'Generated {len(rows)} typed C contract cases; {len(skipped)} JSON-type cases reserved for future parser')

#!/usr/bin/env python3
"""Generate SDK GATT constants from the shared development profile; never starts radio."""
import argparse
import json
from pathlib import Path
from uuid import UUID

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT/'firmware/ble_study/main/gatt_profile.h'


def render(profile):
    for key, expected in {'profile_version':1, 'maximum_active_connections':1,
            'maximum_pending_requests':1, 'maximum_chunk_bytes':512,
            'maximum_payload_bytes':4096, 'maximum_frame_bytes':4112}.items():
        if type(profile[key]) is not int or profile[key] != expected:
            raise ValueError(f'invalid {key}')
    if profile['security'] != 'LESC_MITM_128BIT_ENCRYPTED_WITH_LOCAL_NEW_PEER_ADMISSION':
        raise ValueError('unsupported security profile')
    if profile['frame_deadline_ms'] > profile['request_deadline_ms']:
        raise ValueError('frame deadline exceeds request deadline')
    if profile['request_write'] != 'withResponse' or profile['response_delivery'] != 'indication':
        raise ValueError('unsupported transfer mode')
    values=[]
    for key in ('service_uuid','request_uuid','response_uuid'):
        value=UUID(profile[key])
        if value.version!=4 or str(value)!=profile[key]:raise ValueError('invalid UUID')
        values.append(value)
    if len(set(values))!=3:raise ValueError('duplicate UUID')
    lines=['/* Generated from shared/gatt.profile.json. Development only. */',
           '#ifndef AG_GENERATED_GATT_PROFILE_H','#define AG_GENERATED_GATT_PROFILE_H']
    for name, value in zip(('SERVICE','REQUEST','RESPONSE'),values):
        lines.append(f'#define AG_GATT_{name}_UUID BLE_UUID128_INIT('+', '.join(f'0x{x:02x}' for x in value.bytes[::-1])+')')
    for name,key in [('FRAME_MS','frame_deadline_ms'),('REQUEST_MS','request_deadline_ms')]:
        n=profile[key]
        if type(n) is not int or not 1<=n<=60000:raise ValueError('invalid deadline')
        lines.append(f'#define AG_GATT_{name} {n}u')
    lines += ['#define AG_GATT_CHUNK_BYTES 512u','#define AG_GATT_FRAME_BYTES 4112u','#endif','']
    return '\n'.join(lines)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    options=parser.parse_args()
    content=render(json.loads((ROOT/'shared/gatt.profile.json').read_text()))
    if options.check:
        if not OUTPUT.is_file() or OUTPUT.read_text()!=content:raise SystemExit('Generated GATT header differs')
        print('PASS: SDK GATT constants match shared profile')
    else:
        OUTPUT.parent.mkdir(parents=True,exist_ok=True);OUTPUT.write_text(content)
        print(OUTPUT)

if __name__=='__main__':main()

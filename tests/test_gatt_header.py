import copy
import importlib.util
import json
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
SPEC=importlib.util.spec_from_file_location('ag_gatt_generator',ROOT/'scripts/generate_gatt_header.py')
GEN=importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GEN)


class GATTHeaderTests(unittest.TestCase):
    def setUp(self): self.profile=json.loads((ROOT/'shared/gatt.profile.json').read_text())
    def test_checked_in_header_and_little_endian_service_uuid(self):
        generated=GEN.render(self.profile)
        self.assertEqual((ROOT/'firmware/ble_study/main/gatt_profile.h').read_text(),generated)
        service=next(line for line in generated.splitlines() if line.startswith('#define AG_GATT_SERVICE_UUID'))
        self.assertIn('BLE_UUID128_INIT(0x87, 0x13, 0xef, 0xcc, 0x2c, 0xb1, 0x9e, 0x81, 0xb1, 0x49, 0xa8, 0xd3, 0x0a, 0xdc, 0x43, 0x5c)',service)
    def test_unsupported_profile_fails_closed(self):
        for key,value in [('profile_version',True),('maximum_active_connections',2),
                ('maximum_payload_bytes',8192),('maximum_chunk_bytes',True),
                ('request_write','withoutResponse'),('response_delivery','notification'),
                ('security','none'),('frame_deadline_ms',0),('request_deadline_ms',False),
                ('request_deadline_ms',70000),('frame_deadline_ms',30001),
                ('response_uuid',self.profile['request_uuid']),('service_uuid','invalid')]:
            profile=copy.deepcopy(self.profile);profile[key]=value
            with self.subTest(key=key,value=value),self.assertRaises((ValueError,TypeError)):
                GEN.render(profile)

if __name__=='__main__':unittest.main()

import json
from pathlib import Path
import unittest
from uuid import UUID

ROOT = Path(__file__).resolve().parents[1]


class GATTProfileTests(unittest.TestCase):
    def test_profile_bounds_and_transport_contract(self):
        profile = json.loads((ROOT/'shared/gatt.profile.json').read_text())
        self.assertEqual(profile['profile_version'], 1)
        self.assertEqual(profile['status'], 'DEVELOPMENT_CANDIDATE_NOT_RADIO_VALIDATED')
        ids = [profile[key] for key in ('service_uuid', 'request_uuid', 'response_uuid')]
        self.assertEqual(len(set(ids)), 3)
        for value in ids:
            self.assertEqual(str(UUID(value)), value)
            self.assertEqual(UUID(value).version, 4)
        self.assertEqual(profile['request_write'], 'withResponse')
        self.assertEqual(profile['response_delivery'], 'indication')
        self.assertEqual(profile['maximum_active_connections'], 1)
        self.assertEqual(profile['maximum_pending_requests'], 1)
        self.assertEqual(profile['maximum_frame_bytes'], profile['maximum_payload_bytes']+16)
        self.assertEqual(profile['maximum_payload_bytes'], 4096)
        self.assertEqual(profile['maximum_chunk_bytes'], 512)
        for key in ('frame_deadline_ms', 'request_deadline_ms', 'scan_deadline_ms'):
            self.assertIs(type(profile[key]), int)
            self.assertGreater(profile[key], 0)
        self.assertLessEqual(profile['frame_deadline_ms'], profile['request_deadline_ms'])
        self.assertEqual(profile['security'], 'LESC_MITM_128BIT_ENCRYPTED_WITH_LOCAL_NEW_PEER_ADMISSION')

    def test_mobile_profile_is_generated_from_shared_source(self):
        source = json.loads((ROOT/'shared/gatt.profile.json').read_text())
        resource = json.loads((ROOT/'mobile/Sources/GaugeCore/Resources/gatt.profile.json').read_text())
        self.assertEqual(resource, source)


if __name__ == '__main__': unittest.main()

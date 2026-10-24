"""Shared signed vectors are generated ephemerally, never release artifacts."""
import base64
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
SPEC=importlib.util.spec_from_file_location('ota_vectors',ROOT/'scripts/generate_ota_vectors.py')
VECTORS=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(VECTORS)
sys.path.insert(0,str(ROOT/'shared/reference'))
from esp_image import inspect_image


class OTAVectorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.bundle=VECTORS.generate_vectors()

    def test_expected_profiles_and_real_reference_crypto_rejections(self):
        cases=self.bundle['cases'];self.assertEqual(len(cases),46)
        self.assertEqual(len({case['name'] for case in cases}),46)
        good={'valid','valid_reordered_whitespace','valid_ascii_escape','valid_release_max',
              'valid_escaped_signature','valid_escaped_manifest_key','valid_max_profile'}
        self.assertEqual({c['name'] for c in cases if c['expected_package']},good)
        self.assertEqual({c['name'] for c in cases if c['expected_manifest']},
                         good|{'image_mutated','image_trailing','image_truncated'})
        reference=good|{'key_spki_not_pkcs1','key_der_trailing','key_der_garbage',
                        'esp_version32','esp_image_unaligned'}
        self.assertEqual({c['name'] for c in cases if c['expected_reference']},reference)

    def test_transcripts_and_public_only_bundle(self):
        encoded=json.dumps(self.bundle).encode()
        self.assertNotIn(b'PRIVATE KEY',encoded)
        self.assertIn('BEGIN PUBLIC KEY',self.bundle['public_key_pem'])
        for case in self.bundle['cases']:
            body=base64.b64decode(case['canonical_base64'],validate=True)
            self.assertEqual(hashlib.sha256(body).hexdigest(),case['canonical_sha256'])
            self.assertEqual(len(base64.b64decode(case['signature_base64'],validate=True)),384)
        public=base64.b64decode(self.bundle['public_key_der_base64'],validate=True)
        self.assertLessEqual(len(public),1024);self.assertEqual(public[0],0x30)

    def test_synthetic_image_has_structural_preflight_but_is_not_a_release(self):
        image=base64.b64decode(self.bundle['image_base64'],validate=True)
        info=inspect_image(image,chip_revision=0,inactive_capacity=self.bundle['inactive_capacity'],minimum_secure_version=0)
        self.assertEqual(info.project_name,'TEST-ONLY');self.assertEqual(info.firmware_version,'1.0.0')

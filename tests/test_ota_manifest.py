import base64
import copy
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'shared/reference'))
from ota_manifest import ALGORITHM, MAX_ENVELOPE, PackageError, counter, signing_bytes, verify_package


class OTAPackageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.openssl = shutil.which('openssl')
        if cls.openssl is None:
            raise RuntimeError('OpenSSL required: signed OTA tests must not be skipped')
        cls.temporary = tempfile.TemporaryDirectory(prefix='ag-ota-tests-')
        cls.directory = Path(cls.temporary.name)
        cls.private = cls.directory/'ephemeral-private.pem'
        cls.public = cls.directory/'ephemeral-public.pem'
        subprocess.run([cls.openssl, 'genpkey', '-algorithm', 'RSA', '-pkeyopt', 'rsa_keygen_bits:3072',
                        '-out', str(cls.private)], check=True, capture_output=True)
        subprocess.run([cls.openssl, 'pkey', '-in', str(cls.private), '-pubout', '-out', str(cls.public)],
                       check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def setUp(self):
        self.image = self.directory/'candidate.bin'
        self.image.write_bytes(bytes(range(256))*273)
        self.manifest = dict(schema_version=1, target='esp32s3', hardware_id='TEST-ONLY-NOT-A0',
            firmware_version='0.0.1-test', release_counter='9007199254740993',
            image_bytes=self.image.stat().st_size,
            image_sha256=hashlib.sha256(self.image.read_bytes()).hexdigest(), signing_key_id='ephemeral-test')
        self.envelope = self.sign(self.manifest)

    def sign(self, manifest, payload=None, options=None):
        body = self.directory/'signing-input.bin'
        signature = self.directory/'signature.bin'
        body.write_bytes(signing_bytes(manifest) if payload is None else payload)
        subprocess.run([self.openssl, 'dgst', '-sha256', '-sign', str(self.private), '-out', str(signature),
            *(options or ['-sigopt', 'rsa_padding_mode:pss', '-sigopt', 'rsa_pss_saltlen:32',
                          '-sigopt', 'rsa_mgf1_md:sha256']), str(body)], check=True, capture_output=True)
        return json.dumps(dict(manifest=manifest, signature_algorithm=ALGORITHM,
            signature=base64.b64encode(signature.read_bytes()).decode('ascii'))).encode()

    def verify(self, envelope=None, **kwargs):
        settings = dict(trusted_keys={'ephemeral-test': self.public}, hardware_id='TEST-ONLY-NOT-A0',
                        inactive_capacity=self.manifest['image_bytes'], accepted_counter=7, openssl=self.openssl)
        settings.update(kwargs)
        return verify_package(self.envelope if envelope is None else envelope, self.image, **settings)

    def test_real_signature_and_streaming_digest_preserve_uint64(self):
        verified = self.verify()
        self.assertEqual(verified.release_counter, 9007199254740993)
        self.assertEqual(verified.image_bytes, self.manifest['image_bytes'])
        self.assertEqual(self.image.read_bytes(), bytes(range(256))*273)
        self.assertEqual(verified.image_sha256, self.manifest['image_sha256'])

    def test_trusted_hardware_capacity_and_replay_floor_are_required(self):
        for kwargs in [dict(hardware_id='OTHER-BOARD'), dict(inactive_capacity=self.manifest['image_bytes']-1),
                       dict(inactive_capacity=True), dict(accepted_counter=True), dict(accepted_counter=-1),
                       dict(accepted_counter=9007199254740993), dict(accepted_counter=2**64-1),
                       dict(trusted_keys={}), dict(openssl='ag-openssl-does-not-exist')]:
            with self.subTest(kwargs=kwargs), self.assertRaises(PackageError): self.verify(**kwargs)

    def test_mutated_truncated_and_appended_images_are_rejected(self):
        original = self.image.read_bytes()
        for candidate in [original[:-1], original+b'\0', b'\x01'+original[1:]]:
            self.image.write_bytes(candidate)
            with self.assertRaises(PackageError): self.verify()
        self.image.write_bytes(original)
        self.verify()

    def test_manifest_signature_and_domain_tampering_are_rejected(self):
        value = json.loads(self.envelope)
        value['manifest']['firmware_version'] = '0.0.2-forged'
        with self.assertRaises(PackageError): self.verify(json.dumps(value).encode())
        value = json.loads(self.envelope)
        signature = bytearray(base64.b64decode(value['signature'])); signature[0] ^= 1
        value['signature'] = base64.b64encode(signature).decode()
        with self.assertRaises(PackageError): self.verify(json.dumps(value).encode())
        unprefixed = json.dumps(self.manifest, sort_keys=True, separators=(',', ':')).encode()
        with self.assertRaises(PackageError): self.verify(self.sign(self.manifest, payload=unprefixed))
        with self.assertRaises(PackageError): self.verify(self.sign(self.manifest, options=['-sigopt', 'rsa_padding_mode:pkcs1']))

    def test_whitespace_order_and_key_escapes_do_not_change_signed_meaning(self):
        value = json.loads(self.envelope)
        rearranged = json.dumps(value, indent=4).replace('"hardware_id"', '"hardware\\u005fid"').encode()
        self.assertEqual(self.verify(rearranged), self.verify())

    def test_duplicate_unknown_deep_and_noninteger_json_are_rejected(self):
        samples = [b'{"manifest":{},"manifest":{}}', b'['*5+b']'*5,
                   b'{"x":NaN}', b'{"x":1.0}', b'{"x":1e0}', b'{} garbage', b'\xff', b'', b'x'*(MAX_ENVELOPE+1)]
        value = json.loads(self.envelope); value['unknown'] = 1
        samples.append(json.dumps(value).encode())
        samples.append(self.envelope.replace(b'"signature_algorithm":', b'"signature\\u005falgorithm":"x","signature_algorithm":', 1))
        for sample in samples:
            with self.subTest(sample=sample[:40]), self.assertRaises(PackageError): self.verify(sample)

    def test_manifest_types_schema_and_bounds_fail_before_verification(self):
        cases = [('schema_version', True), ('schema_version', 2), ('target', 'esp32'),
                 ('hardware_id', ''), ('hardware_id', '../board'), ('image_bytes', True),
                 ('image_bytes', 0), ('image_bytes', 16*1024*1024+1), ('image_sha256', 'A'*64),
                 ('signing_key_id', 'x'*65), ('firmware_version', 'version with spaces'),
                 ('release_counter', 8), ('release_counter', '01'), ('release_counter', str(2**64))]
        for key, item in cases:
            value = json.loads(self.envelope); value['manifest'][key] = item
            with self.subTest(key=key, item=item), self.assertRaises(PackageError): self.verify(json.dumps(value).encode())
        value = json.loads(self.envelope); value['manifest']['extra'] = 1
        with self.assertRaises(PackageError): self.verify(json.dumps(value).encode())

    def test_signature_encoding_algorithm_and_wrong_key_are_rejected(self):
        for signature in [True, '', '!', base64.b64encode(b'x'*383).decode(), json.loads(self.envelope)['signature']+'\n']:
            value = json.loads(self.envelope); value['signature'] = signature
            with self.subTest(signature=str(signature)[:20]), self.assertRaises(PackageError): self.verify(json.dumps(value).encode())
        value = json.loads(self.envelope); value['signature_algorithm'] = 'none'
        with self.assertRaises(PackageError): self.verify(json.dumps(value).encode())
        wrong = self.directory/'invalid-public.pem'; wrong.write_text('invalid')
        with self.assertRaises(PackageError): self.verify(trusted_keys={'ephemeral-test': wrong})

    def test_exact_rsa_key_size_and_different_trust_anchor(self):
        original_private = self.private
        for bits in (3070, 3072):
            private = self.directory / f'other-{bits}-private.pem'
            public = self.directory / f'other-{bits}-public.pem'
            subprocess.run([self.openssl, 'genpkey', '-algorithm', 'RSA', '-pkeyopt', f'rsa_keygen_bits:{bits}',
                            '-out', str(private)], check=True, capture_output=True)
            subprocess.run([self.openssl, 'pkey', '-in', str(private), '-pubout', '-out', str(public)],
                           check=True, capture_output=True)
            try:
                self.private = private
                alternate = self.sign(self.manifest)
            finally:
                self.private = original_private
            if bits == 3070:
                # Both keys yield 384-byte signatures; signature length alone is insufficient.
                self.assertEqual(len(base64.b64decode(json.loads(alternate)['signature'])), 384)
                with self.assertRaisesRegex(PackageError, 'exactly RSA-3072'):
                    self.verify(alternate, trusted_keys={'ephemeral-test': public})
            else:
                self.verify(alternate, trusted_keys={'ephemeral-test': public})
                with self.assertRaisesRegex(PackageError, 'signature verification failed'):
                    self.verify(trusted_keys={'ephemeral-test': public})

    def test_terminal_counter_and_failed_verification_do_not_advance_state(self):
        value = copy.deepcopy(self.manifest); value['release_counter'] = str(2**64-1)
        verified = self.verify(self.sign(value), accepted_counter=2**64-2)
        self.assertEqual(verified.release_counter, 2**64-1)
        with self.assertRaises(PackageError): self.verify(self.sign(value), accepted_counter=2**64-1)
        self.assertEqual(counter('0'), 0)
        self.verify()  # verifier has no persistent state or hidden accepted-counter mutation


if __name__ == '__main__': unittest.main()

"""Structural preflight is independent of signature and device validation."""
import base64
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'shared/reference'))
from esp_image import ImageError, inspect_image, verify_candidate
from ota_manifest import ALGORITHM, PackageError, signing_bytes


def fixture():
    header = bytearray(24)
    header[0:4] = bytes((0xE9, 2, 2, 0x20))
    struct.pack_into('<I', header, 4, 0x40370000)
    struct.pack_into('<H', header, 12, 9)
    struct.pack_into('<HH', header, 15, 0, 99)
    header[23] = 1
    descriptor = bytearray(256)
    struct.pack_into('<II', descriptor, 0, 0xABCD5432, 2)
    descriptor[16:21] = b'1.2.3'
    descriptor[48:58] = b'auto_gauge'
    result = header
    checksum = 0xEF
    for address, segment in ((0x3C000020, descriptor), (0x40370000, b'\x01\x02\x03\x04')):
        result += struct.pack('<II', address, len(segment)) + segment
        for value in segment:
            checksum ^= value
    result += bytes(((len(result)+16)//16)*16-len(result)-1) + bytes((checksum,))
    return bytes(result + hashlib.sha256(result).digest())


def rebuild_integrity(data):
    result = bytearray(data)
    offset, checksum = 24, 0xEF
    for _ in range(result[1]):
        length = struct.unpack_from('<I', result, offset+4)[0]
        offset += 8
        for value in result[offset:offset+length]: checksum ^= value
        offset += length
    result[-33] = checksum
    result[-32:] = hashlib.sha256(result[:-32]).digest()
    return bytes(result)


class ImageFormatTests(unittest.TestCase):
    def inspect(self, data=None, **options):
        settings = dict(chip_revision=1, inactive_capacity=4096, minimum_secure_version=2)
        settings.update(options)
        return inspect_image(fixture() if data is None else data, **settings)

    def test_two_segments_and_descriptor(self):
        info = self.inspect()
        self.assertEqual((info.segment_count, info.firmware_version, info.project_name),
                         (2, '1.2.3', 'auto_gauge'))
        self.assertEqual(info.image_sha256, hashlib.sha256(fixture()).hexdigest())

    def test_every_truncation_and_trailing_data_rejected(self):
        data = fixture()
        for size in range(len(data)):
            with self.subTest(size=size), self.assertRaises(ImageError):
                self.inspect(data[:size])
        for suffix in (b'\0', b'\xff'*4096):
            with self.assertRaises(ImageError): self.inspect(data+suffix)

    def test_every_single_byte_mutation_rejected(self):
        data = fixture()
        for index in range(len(data)):
            changed = bytearray(data); changed[index] ^= 1
            with self.subTest(index=index), self.assertRaises(ImageError):
                self.inspect(bytes(changed))

    def test_header_extent_and_bootloader_are_rejected_even_with_valid_hash(self):
        for offset, value in ((0, b'\xe8'), (1, b'\x11'), (12, b'\x00\x00'),
                              (19, b'\x01'), (23, b'\0'), (28, b'\xff'*4),
                              (32, b'\0'*4)):
            changed = bytearray(fixture()); changed[offset:offset+len(value)] = value
            changed[-32:] = hashlib.sha256(changed[:-32]).digest()
            with self.subTest(offset=offset), self.assertRaises(ImageError):
                self.inspect(bytes(changed))

    def test_revision_capacity_secure_floor_and_types(self):
        for options in (dict(chip_revision=100), dict(chip_revision=-1), dict(chip_revision=True),
                        dict(inactive_capacity=len(fixture())-1), dict(inactive_capacity=False),
                        dict(minimum_secure_version=3), dict(minimum_secure_version=-1)):
            with self.subTest(options=options), self.assertRaises(ImageError):
                self.inspect(**options)
        with self.assertRaises(ImageError): self.inspect(bytearray(fixture()))

    def test_checksum_padding_and_descriptor_text_with_rebuilt_hash(self):
        for offset in (len(fixture())-33, len(fixture())-34, 48+5, 80-1):
            changed = bytearray(fixture()); changed[offset] ^= 0x80
            changed[-32:] = hashlib.sha256(changed[:-32]).digest()
            with self.subTest(offset=offset), self.assertRaises(ImageError):
                self.inspect(bytes(changed))

    def test_semantic_descriptor_and_revision_errors_with_valid_integrity(self):
        for offset, value, message in ((48, b'\0', 'application version'),
                (48+31, b'x', 'application version'),
                (80, b'\xff', 'project name'),
                (15, struct.pack('<HH', 90, 20), 'inverted'),
                (36, struct.pack('<I', 1), 'secure version')):
            changed = bytearray(fixture()); changed[offset:offset+len(value)] = value
            with self.subTest(offset=offset), self.assertRaisesRegex(ImageError, message):
                self.inspect(rebuild_integrity(changed))
        for maximum in (0, 65535):
            changed = bytearray(fixture()); struct.pack_into('<H', changed, 17, maximum)
            self.inspect(rebuild_integrity(changed), chip_revision=500)

    def test_cli_requires_all_trust_inputs_when_any_authentication_option_present(self):
        result = subprocess.run([sys.executable, str(ROOT/'shared/reference/esp_image.py'),
            '/path/not-read', '--chip-revision', '0', '--inactive-capacity', '4096',
            '--minimum-secure-version', '0', '--key-id', 'test'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn('signed preflight requires', result.stderr)


class SignedImageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix='ag-esp-sign-test-')
        cls.root = Path(cls.directory.name)
        cls.openssl = shutil.which('openssl')
        if not cls.openssl: raise RuntimeError('OpenSSL required')
        cls.private, cls.public = cls.root/'ephemeral.pem', cls.root/'public.pem'
        subprocess.run([cls.openssl, 'genpkey', '-algorithm', 'RSA', '-pkeyopt',
                        'rsa_keygen_bits:3072', '-out', str(cls.private)], check=True, capture_output=True)
        subprocess.run([cls.openssl, 'pkey', '-in', str(cls.private), '-pubout',
                        '-out', str(cls.public)], check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls): cls.directory.cleanup()

    def setUp(self):
        self.image = self.root/'candidate.bin'; self.image.write_bytes(fixture())
        self.options = dict(chip_revision=1, minimum_secure_version=2,
            trusted_keys={'test': self.public}, hardware_id='TEST-ONLY',
            inactive_capacity=4096, accepted_counter=3, openssl=self.openssl)

    def sign(self, version='1.2.3', image=None):
        data = fixture() if image is None else image
        manifest = dict(schema_version=1, target='esp32s3', hardware_id='TEST-ONLY',
            firmware_version=version, release_counter='4', image_bytes=len(data),
            image_sha256=hashlib.sha256(data).hexdigest(), signing_key_id='test')
        body, sig = self.root/'body', self.root/'sig'
        body.write_bytes(signing_bytes(manifest))
        subprocess.run([self.openssl, 'dgst', '-sha256', '-sign', str(self.private),
            '-out', str(sig), '-sigopt', 'rsa_padding_mode:pss', '-sigopt', 'rsa_pss_saltlen:32',
            '-sigopt', 'rsa_mgf1_md:sha256', str(body)], check=True, capture_output=True)
        return json.dumps(dict(manifest=manifest, signature_algorithm=ALGORITHM,
            signature=base64.b64encode(sig.read_bytes()).decode())).encode()

    def test_signature_and_format_on_same_retained_snapshot(self):
        candidate = verify_candidate(self.sign(), self.image, **self.options)
        self.image.write_bytes(b'changed after validation')
        self.assertEqual(candidate.image, fixture())
        self.assertEqual(candidate.package.image_sha256, candidate.info.image_sha256)
        self.assertNotIn('image=', repr(candidate))

    def test_mismatched_app_version_and_signed_non_image_rejected(self):
        with self.assertRaisesRegex(ImageError, 'version mismatch'):
            verify_candidate(self.sign(version='1.2.4'), self.image, **self.options)
        arbitrary = b'not ESP32 firmware'
        self.image.write_bytes(arbitrary)
        with self.assertRaises(ImageError):
            verify_candidate(self.sign(image=arbitrary), self.image, **self.options)

    def test_changed_path_after_snapshot_does_not_change_verification(self):
        import esp_image
        actual = esp_image.verify_package
        envelope = self.sign()
        def mutate_then_verify(*args, **kwargs):
            self.image.write_bytes(b'external path changed')
            return actual(*args, **kwargs)
        with patch('esp_image.verify_package', side_effect=mutate_then_verify):
            candidate = verify_candidate(envelope, self.image, **self.options)
        self.assertEqual(candidate.image, fixture())
        bad_envelope = bytearray(envelope); bad_envelope[-5] ^= 1
        self.image.write_bytes(fixture())
        with self.assertRaises(PackageError):
            verify_candidate(bytes(bad_envelope), self.image, **self.options)

    def test_signed_cli_passes_and_rejects_changed_image(self):
        envelope = self.root/'envelope.json'; envelope.write_bytes(self.sign())
        command = [sys.executable, str(ROOT/'shared/reference/esp_image.py'), str(self.image),
            '--chip-revision', '1', '--inactive-capacity', '4096', '--minimum-secure-version', '2',
            '--envelope', str(envelope), '--public-key', str(self.public), '--key-id', 'test',
            '--hardware-id', 'TEST-ONLY', '--accepted-counter', '3']
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('SIGNED MANIFEST + FORMAT PREFLIGHT', result.stdout)
        self.assertIn('No Secure Boot', result.stdout)
        self.image.write_bytes(fixture()+b'\0')
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('REJECTED:', result.stderr)


if __name__ == '__main__': unittest.main()

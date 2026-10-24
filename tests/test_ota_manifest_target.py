"""Target C parser/PSA wiring, real ephemeral vectors, explicitly noncrypto PSA model."""
import base64
import copy
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def generate_header(bundle, path):
    cases = copy.deepcopy(bundle['cases'])
    original = base64.b64decode(cases[0]['envelope_base64'])
    for name, packet, expected in [
        ('review_escaped_envelope_key', original.replace(b'"manifest"', b'"\\u006danifest"', 1), True),
        ('review_escaped_manifest_key', original.replace(b'"schema_version"', b'"schema_versi\\u006fn"', 1), True),
        ('review_ascii_unicode_value', original.replace(b'"esp32s3"', b'"esp32\\u00733"', 1), True),
        ('review_duplicate_escaped_envelope', original[:-1]+b',"\\u0073ignature_algorithm":"RSA-PSS-SHA256"}', False),
        ('review_embedded_nul', original.replace(b'"esp32s3"', b'"esp32\\u0000s3"', 1), False),
        ('review_surrogate', original.replace(b'"esp32s3"', b'"\\ud800"', 1), False),
        ('review_bad_escape', original.replace(b'"esp32s3"', b'"esp32\\q3"', 1), False),
        ('review_number_leading_zero', original.replace(b'"schema_version":1', b'"schema_version":01', 1), False),
        ('review_raw_utf8', original.replace(b'"esp32s3"', b'"esp32\xc3\xa9"', 1), False),
    ]:
        case = copy.deepcopy(cases[0])
        case.update(name=name, envelope_base64=base64.b64encode(packet).decode(), expected_manifest=expected)
        cases.append(case)
    output = ['/* Generated ephemeral PUBLIC test material only. */']

    def byte_array(name, value):
        output.append(f'static const uint8_t {name}[] = {{'+
            ','.join(str(byte) for byte in value or b'\0')+'};')
        return name

    entries = []
    for index, case in enumerate(cases):
        envelope = base64.b64decode(case['envelope_base64'])
        canonical = base64.b64decode(case['canonical_base64'])
        signature = base64.b64decode(case['signature_base64'])
        der = base64.b64decode(case.get('key_der_base64', bundle['public_key_der_base64']))
        expected = case['expected_manifest']
        manifest = json.loads(envelope)['manifest'] if expected else {}
        values = [('envelope', envelope), ('canonical', canonical), ('signature', signature), ('der', der),
            ('digest', bytes.fromhex(case['canonical_sha256'])),
            ('image_hash', bytes.fromhex(manifest['image_sha256']) if expected else bytes(32))]
        names = [byte_array(f'v{index}_{field}', value) for field, value in values]
        strings = [case['name'], case.get('hardware_id', bundle['hardware_id']),
            case.get('key_id', bundle['key_id']), manifest.get('firmware_version', '')]
        name, hardware, key, version = map(json.dumps, strings)
        entries.append('{'+f'{name},'+','.join(names)+','+
            f'{len(envelope)},{len(canonical)},{len(signature)},{len(der)},'+
            f'{hardware},{key},{version},{case.get("inactive_capacity", bundle["inactive_capacity"])},'+
            f'{manifest.get("image_bytes",0)},UINT64_C({case.get("accepted_counter",bundle["accepted_counter"])}),'+
            f'UINT64_C({manifest.get("release_counter",0)}),{str(expected).lower()},{str(case["crypto_valid"]).lower()}'+'}')
    output.append('static const manifest_vector_t kManifestVectors[] = {'+',\n'.join(entries)+'};')
    output.append('static const size_t kManifestVectorCount = sizeof(kManifestVectors)/sizeof(kManifestVectors[0]);')
    path.write_text('\n'.join(output)+'\n')
    return len(cases)


class TargetManifestTests(unittest.TestCase):
    def test_actual_target_parser_and_psa_wiring(self):
        compiler = shutil.which(os.environ.get('CC', 'cc'))
        if compiler is None:
            raise RuntimeError('C compiler required for target manifest checks')
        spec = importlib.util.spec_from_file_location('ag_ephemeral_vectors', ROOT/'scripts/generate_ota_vectors.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        bundle = module.generate_vectors()
        with tempfile.TemporaryDirectory(prefix='ag-target-manifest-') as directory:
            temporary = Path(directory)
            vector_count = generate_header(bundle, temporary/'ota_manifest_vectors.h')
            executable = temporary/'manifest_tests'
            subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-UNDEBUG',
                '-I', str(ROOT/'firmware/ota_study/tests/shim'),
                '-I', str(ROOT/'firmware/ota_study/main'), '-I', str(temporary),
                str(ROOT/'firmware/ota_study/main/manifest_verify.c'),
                str(ROOT/'firmware/ota_study/tests/manifest_driver.c'),
                '-o', str(executable)], check=True)
            result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
            self.assertIn('PASS: 4 target manifest groups', result.stdout)
            self.assertIn(f'{vector_count} ephemeral cross-language manifest vectors', result.stdout)


if __name__ == '__main__':
    unittest.main()

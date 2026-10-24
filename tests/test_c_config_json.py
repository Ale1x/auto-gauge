"""Compile bounded C wire-config parser with ASan/UBSan; no device IO."""
import copy
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BASE = json.loads((ROOT / 'shared/fixtures/config.valid.json').read_text())
CASES = json.loads((ROOT / 'shared/fixtures/config.cases.json').read_text())


def wire(value):
    return json.dumps(value, separators=(',', ':'), ensure_ascii=True).encode()


class CConfigJSONTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc = shutil.which(os.environ.get('CC', 'cc'))
        if cc is None:
            raise unittest.SkipTest('C compiler missing: parser not verified')
        cls.temp = tempfile.TemporaryDirectory(prefix='ag-config-json-test-')
        cls.binary = Path(cls.temp.name) / 'config_json'
        subprocess.run([cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                        '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-UNDEBUG',
                        '-I', str(ROOT / 'shared/c'), '-I', str(ROOT / 'firmware/include'),
                        str(ROOT / 'shared/c/ag_config_json.c'), str(ROOT / 'firmware/src/config.c'),
                        str(ROOT / 'tests/config_json_driver.c'), '-o', str(cls.binary)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def invoke(self, data, *args):
        return subprocess.run([str(self.binary), *args], input=data, capture_output=True, timeout=15)

    def parse(self, data, valid, *args):
        result = self.invoke(data, 'parse', *args)
        self.assertEqual(result.returncode, 0 if valid else 2, result.stderr.decode(errors='replace'))
        if valid:
            self.assertEqual(json.loads(result.stdout), json.loads(data))
        return result

    def test_all_22_shared_cases_and_golden(self):
        self.assertEqual(len(CASES), 22)
        self.parse((ROOT / 'shared/fixtures/config.valid.json').read_bytes(), True)
        for case in CASES:
            with self.subTest(case=case['name']):
                self.parse(wire(case['configuration']), case['valid'])

    def test_capacity_pid_authority_and_all_fifteen_widgets(self):
        value = copy.deepcopy(BASE)
        value['revision'] = 2147483647
        widget = value['dashboards'][0]['widgets'][0]
        widget['pid'] = 222  # Beyond demo catalog, accepted only via caller capability mask.
        value['dashboards'] = [dict(id=d, widgets=[dict(copy.deepcopy(widget), id=w) for w in range(1, 6)]) for d in range(1, 4)]
        self.parse(wire(value), True)
        self.parse(wire(value), False, 'none')
        value['dashboards'][2]['widgets'].append(copy.deepcopy(widget))
        self.parse(wire(value), False)

    def test_exact_byte_budget_and_length_delimited_nul(self):
        data = wire(BASE)
        self.parse(data + b' ' * (4096 - len(data)), True)
        self.parse(data + b' ' * (4097 - len(data)), False)
        for raw in (b'', b'\x00', data + b'\x00', data + b'{}', b'\xef\xbb\xbf' + data):
            self.parse(raw, False)
        result = self.invoke(b'', 'selftest')
        self.assertEqual(result.returncode, 0, result.stderr.decode(errors='replace'))

    def test_unknown_missing_duplicate_keys_at_every_object(self):
        data = wire(BASE)
        for key in ('schema_version', 'revision', 'brightness', 'dashboards'):
            value = copy.deepcopy(BASE); value.pop(key)
            self.parse(wire(value), False)
        objects = [BASE, BASE['brightness'], BASE['dashboards'][0], BASE['dashboards'][0]['widgets'][0], BASE['dashboards'][0]['widgets'][0]['rect']]
        for obj in objects:
            original = wire(obj)
            # Replace only the first corresponding object in the base document.
            with self.subTest(object=next(iter(obj))):
                missing = dict(obj); missing.pop(next(iter(obj)))
                self.parse(data.replace(original, wire(missing), 1), False)
                self.parse(data.replace(original, b'{"extra":0,' + original[1:], 1), False)
                key = next(iter(obj)); member = json.dumps(key).encode() + b':' + wire(obj[key])
                self.parse(data.replace(original, b'{' + member + b',' + original[1:], 1), False)
                # The escaped spelling must decode to the same duplicate key.
                escaped = ('"\\u%04x%s"' % (ord(key[0]), key[1:])).encode()
                duplicate = escaped + b':' + wire(obj[key])
                self.parse(data.replace(original, b'{' + duplicate + b',' + original[1:], 1), False)

    def test_ascii_escapes_supported_nonascii_controls_rejected(self):
        data = wire(BASE)
        self.parse(data.replace(b'"schema_version"', b'"\\u0073chema_version"').replace(b'"manual"', b'"man\\u0075al"'), True)
        for spelling in (b'"\\u00e9"', b'"\\ud83d\\ude00"', b'"\\ud800"', b'"\\u0000"', b'"man\\nual"', b'"\\q"', b'"\\u12"', b'"\\u00xy"', b'"\xc3\xa9"', b'"' + b'x' * 32 + b'"'):
            self.parse(data.replace(b'"manual"', spelling), False)
        self.parse(data.replace(b'"x":2500', b'"\\u0078":2500'), True)

    def test_number_grammar_types_boundaries(self):
        data = wire(BASE)
        for number in (b'true', b'false', b'null', b'"1"', b'1.0', b'1e0', b'1E+0', b'+1', b'01', b'-01', b'--1', b'1x', b'2147483648', b'-2147483649', b'9' * 500, b'[]', b'{}'):
            self.parse(data.replace(b'"revision":1', b'"revision":' + number), False)
        for number in (b'-2147483648', b'2147483647'):
            self.parse(data.replace(b'"x":2500', b'"x":' + number), False)
        self.parse(data.replace(b'"x":2500', b'"x":-0'), True)

    def test_malformed_containers_separators_and_nesting(self):
        data = wire(BASE)
        samples = [b'[]', b'null', b'true', b'1', b'"{}"', data[:-1] + b',}',
                   data.replace(b'"revision":1', b'"revision" 1'), data.replace(b',"revision"', b' "revision"'),
                   data.replace(b'"widgets":[', b'"widgets":[,'), data.replace(b'"dashboards":[', b'"dashboards":[' * 10),
                   data.replace(b'"x":2500', b'"x":' + b'[' * 64 + b'1' + b']' * 64)]
        for sample in samples:
            self.parse(sample, False)
        reordered = {key: BASE[key] for key in reversed(BASE)}
        self.parse(b' \r\n\t' + wire(reordered) + b' \r\n\t', True)

    def test_truncations_byte_mutations_random_bounded_inputs_and_output_atomicity(self):
        result = self.invoke(wire(BASE), 'stress')
        self.assertEqual(result.returncode, 0, result.stderr.decode(errors='replace'))


if __name__ == '__main__':
    unittest.main()

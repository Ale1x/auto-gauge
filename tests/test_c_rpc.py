"""Typed RPC codec C/Python comparisons. No controller, network or authorization."""
import copy
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
GOLDEN = json.loads((ROOT/'shared/fixtures/rpc.exchanges.json').read_text())
CONFIG_CASES = json.loads((ROOT/'shared/fixtures/config.cases.json').read_text())
SESSION = '05050505050505050505050505050505'
PAGINATION = json.loads((ROOT/'shared/fixtures/pagination.cases.json').read_text())
MAX64 = (1 << 64) - 1
FIRST_EXTREME = MAX64 - 19
EXTREME_RECORDS = [dict(sequence=str(FIRST_EXTREME+i), monotonic_ms=str(MAX64), pid=255,
    ecu_source=0x1fffffff, status='valid', raw_hex='ff'*64) for i in range(20)]
HISTORY_FIXTURES = [('golden', GOLDEN['history_source'])] + [(c['name'], c) for c in PAGINATION]
HISTORY_FIXTURES += [('max_uint64', dict(session_id=SESSION, capacity=64, first_sequence=str(FIRST_EXTREME), records=EXTREME_RECORDS)),
    ('envelope_edge', dict(session_id=SESSION, capacity=64, first_sequence=str(FIRST_EXTREME), records=[dict(r, raw_hex='ff'*(48 if i == 0 else 64)) for i, r in enumerate(EXTREME_RECORDS)])),
    ('exact_envelope', dict(session_id=SESSION, capacity=64, first_sequence=str(FIRST_EXTREME), records=[dict(r, raw_hex='ff'*(36 if i == 0 else 64)) for i, r in enumerate(EXTREME_RECORDS)])),
    ('empty_loss', dict(session_id=SESSION, capacity=4, first_sequence='10', records=[]))]


def history_header(path):
    # Trusted build fixtures -> typed records only. No encoded golden pages or wire parser.
    def u64(n):
        return 'UINT64_C(%d)' % int(n)
    def byte_array(value):
        return '{' + (','.join('0x%02x' % b for b in bytes.fromhex(value)) or '0') + '}'
    snapshots = []
    for name, source in HISTORY_FIXTURES:
        records = source['records'][-source['capacity']:]
        rows = []
        for r in records:
            rows.append('{.sequence=' + u64(r['sequence']) + ',.monotonic_ms=' + u64(r['monotonic_ms']) +
                ',.ecu_source=' + str(r['ecu_source']) + ',.pid=' + str(r['pid']) + ',.raw_length=' + str(len(r['raw_hex'])//2) +
                ',.status=AG_SAMPLE_' + r['status'].upper() + ',.raw=' + byte_array(r['raw_hex']) + '}')
        latest = int(records[-1]['sequence']) if records else int(source['first_sequence']) - 1
        exhausted = bool(records) and latest == MAX64
        snapshots.append('{.session_id=' + byte_array(source['session_id']) + ',.records={' + (','.join(rows) or '{0}') +
            '},.next_sequence=' + u64(latest if exhausted else latest+1) + ',.last_ms=' + u64(records[-1]['monotonic_ms'] if records else 0) +
            ',.capacity=' + str(source['capacity']) + ',.count=' + str(len(records)) + ',.exhausted=' + ('true' if exhausted else 'false') + '}')
    path.write_text('/* Build-only typed fixtures, not a provisioning or wire endpoint. */\n' +
        'static const ag_history_snapshot_t kRPCSnapshots[]={' + ',\n'.join(snapshots) + '};\n' +
        'static const char *const kRPCSnapshotNames[]={' + ','.join(json.dumps(name) for name, _ in HISTORY_FIXTURES) + '};\n')


def wire(value):
    return json.dumps(value, separators=(',', ':')).encode()


class CRPCTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which(os.environ.get('CC', 'cc'))
        if compiler is None:
            raise RuntimeError('C compiler required for RPC checks')
        cls.temp = tempfile.TemporaryDirectory(prefix='ag-rpc-test-')
        cls.binary = Path(cls.temp.name)/'rpc_codec'
        history_header(Path(cls.temp.name)/'rpc_history_fixtures.h')
        subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                        '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-UNDEBUG',
                        '-I', str(ROOT/'shared/c'), '-I', str(ROOT/'firmware/include'), '-I', cls.temp.name,
                        str(ROOT/'shared/c/ag_rpc.c'), str(ROOT/'shared/c/ag_config_json.c'),
                        str(ROOT/'firmware/src/config.c'), str(ROOT/'firmware/src/history.c'), str(ROOT/'tests/rpc_driver.c'), '-o', str(cls.binary)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def invoke(self, data, *args):
        return subprocess.run([str(self.binary), *args], input=data, capture_output=True, timeout=15)

    def parse(self, value, valid=True, mask='all'):
        data = value if isinstance(value, bytes) else wire(value)
        result = self.invoke(data, 'parse', mask)
        self.assertEqual(result.returncode, 0 if valid else 2, result.stderr.decode(errors='replace'))
        if valid:
            return json.loads(result.stdout)

    def test_ten_golden_requests_decode_to_typed_values(self):
        self.assertEqual(len(GOLDEN['exchanges']), 10)
        for case in GOLDEN['exchanges']:
            request = case['request']
            with self.subTest(id=case['request_id']):
                parsed = self.parse(request)
                self.parse({key: request[key] for key in reversed(request)})
                self.assertEqual(parsed['op'], request['op'])
                if 'configuration' in request:
                    self.assertEqual(parsed['configuration_response']['configuration'], request['configuration'])
                if 'expected_revision' in request:
                    self.assertEqual(parsed['expected_revision'], request['expected_revision'])
                if 'session_id' in request:
                    self.assertEqual(parsed['session_id'], request['session_id'])
                    self.assertEqual(parsed['sequence'], request.get('after_sequence', request.get('acknowledged_through')))
                if 'limit' in request:
                    self.assertEqual(parsed['limit'], request['limit'])

    def test_all_22_config_cases_in_both_operations(self):
        self.assertEqual(len(CONFIG_CASES), 22)
        for case in CONFIG_CASES:
            for op in ('config.validate', 'config.apply'):
                request = dict(op=op, configuration=case['configuration'])
                if op == 'config.apply':
                    request['expected_revision'] = 1
                with self.subTest(case=case['name'], op=op):
                    self.parse(request, case['valid'])
        self.parse(dict(op='config.validate', configuration=GOLDEN['initial_configuration']), False, 'none')
        maximum = copy.deepcopy(GOLDEN['initial_configuration'])
        widget = maximum['dashboards'][0]['widgets'][0]
        widget['pid'] = 222
        maximum['dashboards'] = [dict(id=d, widgets=[dict(copy.deepcopy(widget), id=w) for w in range(1, 6)]) for d in range(1, 4)]
        self.parse(dict(op='config.validate', configuration=maximum))
        encoded = self.invoke(wire(maximum), 'reply', 'current')
        self.assertEqual(encoded.returncode, 0, encoded.stderr.decode(errors='replace'))
        self.assertEqual(json.loads(encoded.stdout)['configuration'], maximum)

    def test_golden_response_encoding_supported_shapes(self):
        compared = 0
        for exchange in GOLDEN['exchanges']:
            for response in exchange['responses']:
                payload = wire(exchange['request'])
                if response.get('phase') == 'current':
                    kind = 'current'; payload = wire(response['configuration'])
                elif response.get('phase') == 'rejected':
                    kind = response['error']
                elif response.get('op') == 'logs.read':
                    kind = 'read'
                elif response.get('op') == 'logs.ack':
                    kind = 'ack'
                else:
                    kind = response.get('phase', response.get('op'))
                result = self.invoke(payload, 'reply', kind)
                self.assertEqual(result.returncode, 0, result.stderr.decode(errors='replace'))
                self.assertEqual(json.loads(result.stdout), response)
                self.assertLessEqual(len(result.stdout.rstrip(b'\n')), 4096)
                compared += 1
        self.assertEqual(compared, 12)

    def test_exact_field_sets_unknown_ops_duplicates_and_ascii_aliases(self):
        for exchange in GOLDEN['exchanges']:
            request = exchange['request']; data = wire(request)
            for key in request:
                candidate = dict(request); candidate.pop(key)
                self.parse(candidate, False)
            self.parse(dict(request, extra=1), False)
            self.parse(b'{"op":"hello",' + data[1:], False)
            self.parse(b'{"\\u006fp":"hello",' + data[1:], False)
        for op in ('can.raw', 'reset', 'owner.transfer', 'cloud.login', 'ota.begin', '', 'HELLO'):
            self.parse(dict(op=op), False)
        self.parse(b'{"\\u006fp":"hel\\u006co"}')
        self.parse({'op': 'hello', 'session_id': SESSION}, False)
        self.parse({'op': 'logs.read', 'session_id': SESSION, 'after_sequence': '0', 'acknowledged_through': '1', 'limit': 1}, False)

    def test_uint64_session_and_int32_strict_boundaries(self):
        for op, counter in (('logs.read', 'after_sequence'), ('logs.ack', 'acknowledged_through')):
            for value in ('0', '9007199254740993', '18446744073709551615'):
                request = dict(op=op, session_id=SESSION, **{counter: value})
                if op == 'logs.read':
                    request['limit'] = 16
                self.assertEqual(self.parse(request)['sequence'], value)
            for value in ('', '00', '01', '-1', '+1', '1.0', '1e0', '18446744073709551616', '9'*64, 0, True, None):
                request = dict(op=op, session_id=SESSION, **{counter: value})
                if op == 'logs.read':
                    request['limit'] = 1
                self.parse(request, False)
        for value in ('0'*32, SESSION.upper().replace('05', 'aB'), 'a'*31, 'a'*33, 'g'*32, None, 42):
            self.parse(dict(op='logs.ack', session_id=value, acknowledged_through='0'), False)
        request = dict(op='config.apply', configuration=GOLDEN['initial_configuration'], expected_revision=2147483647)
        self.parse(request)
        for value in (0, -1, 2147483648, '1', True, None, 1.0):
            self.parse(dict(request, expected_revision=value), False)
        read = dict(op='logs.read', session_id=SESSION, after_sequence='0', limit=1)
        for value in (0, -1, 17, '1', True, None, 1.0):
            self.parse(dict(read, limit=value), False)

    def test_malformed_budget_depth_tokens_and_string_subset(self):
        data = wire(dict(op='hello'))
        self.parse(data + b' '*(4096-len(data)))
        self.parse(data + b' '*(4097-len(data)), False)
        for value in (b'', b'[]', b'null', b'{}', b'{"op":"hello",}', b'{"op" "hello"}', data+b'{}', data+b'\x00', b'\xef\xbb\xbf'+data,
                      b'{"op":"\\u00e9"}', b'{"op":"\\ud800"}', b'{"op":"\\u0000"}', b'{"op":"hel\\nlo"}', b'{"op":"\\q"}', b'{"op":"' + b'x'*65 + b'"}'):
            self.parse(value, False)
        for value in (b'['*20+b'1'+b']'*20, b'['+b','.join([b'0']*600)+b']'):
            self.parse(b'{"op":"config.validate","configuration":'+value+b'}', False)
        nested = copy.deepcopy(GOLDEN['initial_configuration']); nested['brightness']['extra'] = 1
        self.parse(dict(op='config.validate', configuration=nested), False)
        escaped = wire(dict(op='config.validate', configuration=GOLDEN['initial_configuration'])).replace(b'"x":2500', b'"x":2500,"\\u0078":2500')
        self.parse(escaped, False)

    def read_page(self, name, session, after, limit=16, mode=None, valid=True):
        request = dict(op='logs.read', session_id=session, after_sequence=str(after), limit=limit)
        args = ['reply', 'read', name]
        if mode:
            args.append(mode)
        result = self.invoke(wire(request), *args)
        self.assertEqual(result.returncode, 0 if valid else 2, result.stderr.decode(errors='replace'))
        if valid:
            value = json.loads(result.stdout)
            self.assertEqual(set(value), {'simulation', 'op', 'page'})
            self.assertTrue(value['simulation'])
            self.assertEqual(value['op'], 'logs.read')
            self.assertLessEqual(len(result.stdout.rstrip(b'\n')), 4096)
            return value['page']

    def test_all_shared_history_pages_use_typed_snapshots(self):
        for case in PAGINATION:
            for read in case['reads']:
                with self.subTest(case=case['name'], after=read['after_sequence']):
                    page = self.read_page(case['name'], case['session_id'], read['after_sequence'], read['limit'])
                    self.assertEqual(page, read['page'])

    def test_history_uint64_maximum_raw_and_envelope_budget_reduces_records(self):
        page = self.read_page('max_uint64', SESSION, FIRST_EXTREME - 1)
        self.assertEqual(len(page['records']), 15)
        self.assertTrue(all(len(r['raw_hex']) == 128 and r['monotonic_ms'] == str(MAX64) for r in page['records']))
        self.assertEqual(page['lost'], [])
        second = self.read_page('max_uint64', SESSION, page['records'][-1]['sequence'])
        combined = page['records'] + second['records']
        self.assertEqual([int(r['sequence']) for r in combined], list(range(FIRST_EXTREME, MAX64+1)))
        self.assertEqual(self.read_page('max_uint64', SESSION, MAX64)['records'], [])
        edge = dict(HISTORY_FIXTURES)['envelope_edge']
        would_fit_alone = dict(session_id=SESSION, records=edge['records'][:15], lost=[dict(first=str(FIRST_EXTREME-1), last=str(FIRST_EXTREME-1))])
        self.assertLessEqual(len(wire(would_fit_alone)), 4096)
        self.assertGreater(len(wire(dict(simulation=True, op='logs.read', page=would_fit_alone))), 4096)
        actual = self.read_page('envelope_edge', SESSION, FIRST_EXTREME-2)
        self.assertEqual(len(actual['records']), 14)  # Whole envelope, not just internal page.
        self.assertEqual(actual['lost'], would_fit_alone['lost'])
        self.assertEqual(actual['records'], edge['records'][:14])
        tail = self.read_page('envelope_edge', SESSION, actual['records'][-1]['sequence'])
        self.assertEqual(actual['records'] + tail['records'], edge['records'])
        self.assertEqual(tail['lost'], [])
        request = dict(op='logs.read', session_id=SESSION, after_sequence=str(FIRST_EXTREME-2), limit=16)
        exact = self.invoke(wire(request), 'reply', 'read', 'exact_envelope')
        self.assertEqual(exact.returncode, 0, exact.stderr.decode(errors='replace'))
        self.assertEqual(len(exact.stdout.rstrip(b'\n')), 4096)
        self.assertEqual(len(json.loads(exact.stdout)['page']['records']), 15)
        # 'false' is one byte longer than 'true': identical15record page would be4097.
        real = self.invoke(wire(request), 'reply', 'read', 'exact_envelope', 'real')
        self.assertEqual(real.returncode, 0, real.stderr.decode(errors='replace'))
        self.assertFalse(json.loads(real.stdout)['simulation'])
        self.assertEqual(len(json.loads(real.stdout)['page']['records']), 14)
        self.assertLessEqual(len(real.stdout.rstrip(b'\n')), 4096)
        self.read_page('exact_envelope', SESSION, FIRST_EXTREME-2, mode='exact', valid=False)

    def test_history_empty_loss_bad_source_cursor_and_output_short_preserve_output(self):
        self.assertEqual(self.read_page('empty_loss', SESSION, 0), dict(session_id=SESSION, records=[], lost=[dict(first='1', last='9')]))
        self.assertEqual(self.read_page('empty_loss', SESSION, 9), dict(session_id=SESSION, records=[], lost=[]))
        self.read_page('empty_loss', SESSION, 10, valid=False)
        self.read_page('golden', SESSION, 7, valid=False)
        self.read_page('golden', '06'*16, 0, valid=False)
        self.read_page('invalid', SESSION, 0, valid=False)
        self.read_page('null', SESSION, 0, valid=False)
        for mode in ('short', 'exact'):
            self.read_page('golden', SESSION, 0, mode=mode, valid=False)
            self.read_page('empty_loss', SESSION, 9, mode=mode, valid=False)

    def test_output_transactional_failures_and_explicit_response_assertions(self):
        result = self.invoke(b'', 'selftest')
        self.assertEqual(result.returncode, 0, result.stderr.decode(errors='replace'))
        for mode in ('short', 'exact'):
            result = self.invoke(b'', 'reply', 'hello', mode)
            self.assertEqual(result.returncode, 2, result.stderr.decode(errors='replace'))
        result = self.invoke(b'', 'reply', 'durable')
        self.assertEqual(json.loads(result.stdout), dict(simulation=False, phase='applied', applied_revision=2, persistence='durable'))

    def test_truncations_byte_mutations_and_1024_bounded_random_inputs(self):
        for value in ({'op': 'hello'}, {'op': 'config.apply', 'configuration': GOLDEN['initial_configuration'], 'expected_revision': 1}):
            result = self.invoke(wire(value), 'stress')
            self.assertEqual(result.returncode, 0, result.stderr.decode(errors='replace'))


if __name__ == '__main__':
    unittest.main()

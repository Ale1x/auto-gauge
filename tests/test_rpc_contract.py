import copy
import json
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'shared/reference'))
from framing import Frame,decode,encode
from simulator import DeviceSimulator
from log_source import LogPageSource

GOLDEN=json.loads((ROOT/'shared/fixtures/rpc.exchanges.json').read_text())


class RPCContractTests(unittest.TestCase):
    def request(self,sim,request,request_id=1):
        frames=sim.handle(encode(Frame(1,request_id,json.dumps(request,separators=(',',':')).encode())))
        for packet in frames:
            self.assertEqual(decode(packet).request_id,request_id)
            self.assertEqual(decode(packet).kind,2)
        return [json.loads(decode(packet).payload) for packet in frames]

    def test_golden_exchanges_are_reproducible(self):
        history=GOLDEN['history_source']
        source=LogPageSource(history['session_id'],history['capacity'],int(history['first_sequence']))
        for r in history['records']:
            source.append(r['pid'],r['ecu_source'],int(r['monotonic_ms']),r['status'],bytes.fromhex(r['raw_hex']))
        sim=DeviceSimulator(GOLDEN['initial_configuration'],source)
        for exchange in GOLDEN['exchanges']:
            with self.subTest(op=exchange['request']['op']):
                self.assertEqual(self.request(sim,exchange['request'],exchange['request_id']),exchange['responses'])

    def test_missing_applied_ack_does_not_mean_missing_commit(self):
        sim=DeviceSimulator(GOLDEN['initial_configuration'])
        config=copy.deepcopy(sim.configuration);config['revision']+=1
        replies=self.request(sim,dict(op='config.apply',configuration=config,expected_revision=1))
        self.assertEqual(replies[0]['phase'],'received')
        # Model channel loss of replies[1], followed by a new connection/read.
        current=self.request(sim,dict(op='config.get'),2)[0]
        self.assertEqual(current['configuration']['revision'],2)
        retry=self.request(sim,dict(op='config.apply',configuration=config,expected_revision=1),3)
        self.assertEqual(retry[-1]['error'],'revision_conflict')

    def test_history_rpc_includes_envelope_in_4096_budget(self):
        high=(1<<64)-1
        for raw_length in range(1,65):
            source=LogPageSource('05'*16,64,high-63)
            for i in range(64):source.append(255,0x1fffffff,high-63+i,'valid',bytes(range(raw_length)))
            sim=DeviceSimulator(GOLDEN['initial_configuration'],source)
            packets=sim.handle(encode(Frame(1,1,json.dumps(dict(op='logs.read',session_id=source.session_id,after_sequence='0',limit=16)).encode())))
            payload=decode(packets[0]).payload
            self.assertLessEqual(len(payload),4096)
            page=json.loads(payload)['page']
            self.assertTrue(page['records'])
            self.assertLessEqual(len(page['records']),16)

    def test_history_bad_parameters_do_not_delete_source(self):
        source=LogPageSource('05'*16);source.append(12,2024,0,'valid',b'x')
        sim=DeviceSimulator(GOLDEN['initial_configuration'],source)
        for args in [dict(after_sequence=True,limit=16),dict(after_sequence='0',limit=True),dict(after_sequence='2',limit=16)]:
            reply=self.request(sim,dict(op='logs.read',session_id=source.session_id,**args))[0]
            self.assertEqual(reply['error'],'invalid_history_request')
        self.assertEqual(self.request(sim,dict(op='logs.ack',session_id=source.session_id,acknowledged_through='1'))[0]['source_records_deleted'],False)
        self.assertEqual(len(source.records),1)


if __name__=='__main__':unittest.main()

import json
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'shared/reference'))
from log_source import LogPageSource


class PaginationContractTests(unittest.TestCase):
    def test_common_pagination_cases(self):
        cases=json.loads((ROOT/'shared/fixtures/pagination.cases.json').read_text())
        for case in cases:
            with self.subTest(case=case['name']):
                source=LogPageSource(case['session_id'],case['capacity'],int(case['first_sequence']))
                for r in case['records']:
                    self.assertEqual(str(source.next_sequence),r['sequence'])
                    source.append(r['pid'],r['ecu_source'],int(r['monotonic_ms']),r['status'],bytes.fromhex(r['raw_hex']))
                for read in case['reads']:
                    self.assertEqual(source.read_page(read['after_sequence'],read['limit']),read['page'])
                for ack in case['acknowledgements']:
                    source.acknowledge(source.session_id,ack['through'])
                    self.assertEqual(len(source.records),ack['record_count'])


if __name__=='__main__':unittest.main()

import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'shared/reference'))
from log_import import LogArchive
from log_source import LogPageSource

SESSION='03'*16


class BoundedSourceTests(unittest.TestCase):
    def append(self,source,n):
        for _ in range(n):source.append(12,2024,source.next_sequence*1000,'valid',b'\x1a\x10')

    def test_ack_from_a_preserves_history_for_b(self):
        source=LogPageSource(SESSION,4);self.append(source,4)
        with tempfile.TemporaryDirectory() as temp:
            a=LogArchive(str(Path(temp)/'a.sqlite'));b=LogArchive(str(Path(temp)/'b.sqlite'))
            try:
                ack=a.import_page('gauge',source.read_page('0'))
                source.acknowledge(SESSION,ack['acknowledged_through'])
                self.assertEqual(len(source.records),4)
                self.assertEqual(b.import_page('gauge',source.read_page('0'))['acknowledged_through'],'4')
                self.append(source,2)
                page_a=source.read_page('4')
                self.assertEqual(page_a['lost'],[])
                self.assertEqual(a.import_page('gauge',page_a)['acknowledged_through'],'6')
                self.assertEqual(source.read_page('0')['lost'],[dict(first='1',last='2')])
            finally:a.close();b.close()

    def test_loss_is_trimmed_to_client_cursor_and_pagination_is_idempotent(self):
        source=LogPageSource(SESSION,3);self.append(source,5)
        page=source.read_page('1',1)
        self.assertEqual(page['lost'],[dict(first='2',last='2')])
        self.assertEqual([r['sequence'] for r in page['records']],['3'])
        self.assertEqual(source.read_page('1',1),page)
        page['records'][0]['raw_hex']='ffff'
        self.assertEqual(source.read_page('1',1)['records'][0]['raw_hex'],'1a10')

    def test_ring_page_and_invalid_inputs_are_bounded(self):
        source=LogPageSource(SESSION,64)
        for i in range(1000):source.append(12,2024,i,'valid',bytes(range(64)))
        self.assertEqual(len(source.records),64)
        page=source.read_page('0')
        self.assertLessEqual(len(page['records']),16)
        self.assertLessEqual(len(json.dumps(page,separators=(',',':')).encode()),4096)
        with self.assertRaises(ValueError):source.read_page('1001')
        with self.assertRaises(ValueError):source.read_page('0',17)
        with self.assertRaises(ValueError):source.acknowledge('04'*16,'1000')
        with self.assertRaises(ValueError):source.append(12,2024,1,'valid',b'x')
        self.assertEqual(source.next_sequence,1001)


if __name__=='__main__':unittest.main()

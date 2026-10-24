import copy
import json
from pathlib import Path
import sys
import sqlite3
import tempfile
import threading
import tracemalloc
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'shared/reference'))
from framing import Frame,decode,encode,StreamDecoder
from simulator import DeviceSimulator
from log_import import LogArchive

CONFIG=json.loads((ROOT/'shared/fixtures/config.valid.json').read_text())
SESSION='01'*16


def record(seq,status='valid'):
    return dict(sequence=str(seq),monotonic_ms=str(seq*1000),pid=12,ecu_source=2024,
                status=status,raw_hex='1a10' if status=='valid' else '')


def page(*sequences,lost=None,session=SESSION):
    return dict(session_id=session,records=[record(s) for s in sequences],lost=lost or [])


class SimulatorTests(unittest.TestCase):
    def request(self,sim,operation,**fields):
        packet=encode(Frame(1,12,json.dumps(dict(op=operation,**fields),ensure_ascii=False).encode()))
        # Same bytes crossing transport fragments; no BLE is started.
        decoder=StreamDecoder();frames=[]
        for offset in range(0,len(packet),20):frames+=decoder.feed(packet[offset:offset+20],offset//20)
        self.assertEqual(len(frames),1)
        return [json.loads(decode(x).payload) for x in sim.handle(encode(frames[0]))]

    def test_config_received_is_distinct_from_applied_volatile(self):
        sim=DeviceSimulator(CONFIG);candidate=copy.deepcopy(CONFIG);candidate['revision']=2
        replies=self.request(sim,'config.apply',configuration=candidate,expected_revision=1)
        self.assertEqual([r['phase'] for r in replies],['received','applied'])
        self.assertTrue(all(r['simulation'] for r in replies))
        self.assertEqual(replies[-1]['persistence'],'volatile')
        self.assertEqual(sim.configuration['revision'],2)

    def test_invalid_or_conflicting_config_keeps_current(self):
        sim=DeviceSimulator(CONFIG);candidate=copy.deepcopy(CONFIG);candidate['revision']=2
        replies=self.request(sim,'config.apply',configuration=candidate,expected_revision=0)
        self.assertEqual(replies[-1]['error'],'revision_conflict')
        candidate['dashboards']=[]
        self.assertEqual(self.request(sim,'config.apply',configuration=candidate,expected_revision=1)[-1]['error'],'invalid_configuration')
        self.assertEqual(sim.configuration,CONFIG)

    def test_mutations_not_implemented_and_duplicate_json_key_rejected(self):
        sim=DeviceSimulator(CONFIG)
        for op in ('ota.begin','ownership.claim','can.send'):
            self.assertEqual(self.request(sim,op)[0]['error'],'not_implemented')
        raw=encode(Frame(1,1,b'{"op":"hello","op":"config.apply"}'))
        self.assertEqual(json.loads(decode(sim.handle(raw)[0]).payload)['error'],'invalid_message')

    def test_unicode_extensions_cannot_make_applied_state_unreadable(self):
        sim=DeviceSimulator(CONFIG);candidate=copy.deepcopy(CONFIG)
        candidate.update(revision=2,extra='à'*700)
        self.assertEqual(self.request(sim,'config.apply',configuration=candidate,expected_revision=1)[-1]['phase'],'applied')
        current=self.request(sim,'config.get')[0]['configuration']
        self.assertEqual(current['revision'],2)
        self.assertNotIn('extra',current)


class LogImportTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='ag-log-')
        self.path=str(Path(self.temp.name)/'archive.sqlite')
        self.archive=LogArchive(self.path)

    def tearDown(self):self.archive.close();self.temp.cleanup()

    def count(self):return self.archive.db.execute('SELECT count(*) FROM records').fetchone()[0]

    def test_dedup_resume_and_session_isolation(self):
        self.assertEqual(self.archive.import_page('demo',page(1,2))['acknowledged_through'],'2')
        self.archive.close();self.archive=LogArchive(self.path)
        self.archive.import_page('demo',page(1,2))
        self.archive.import_page('demo',page(2,3))
        self.assertEqual(self.count(),3)
        self.archive.import_page('demo',page(1,session='02'*16))
        self.assertEqual(self.archive.cursor('demo',SESSION),3)
        self.assertEqual(self.count(),4)

    def test_gap_must_be_explicit_and_persisted(self):
        with self.assertRaises(ValueError):self.archive.import_page('demo',page(3))
        self.assertEqual(self.count(),0)
        loss=[dict(first='1',last='2')]
        ack=self.archive.import_page('demo',page(3,lost=loss))
        self.assertEqual(ack['acknowledged_through'],'3')
        self.assertEqual(ack['reported_loss'],loss)
        self.assertEqual(self.archive.db.execute('SELECT count(*) FROM losses').fetchone()[0],1)
        with self.assertRaises(ValueError):self.archive.import_page('demo',page(1))

    def test_conflicting_duplicate_rolls_back_whole_page_and_cursor(self):
        self.archive.import_page('demo',page(1,2))
        bad=page(2,3);bad['records'][0]['raw_hex']='ffff'
        with self.assertRaises(ValueError):self.archive.import_page('demo',bad)
        self.assertEqual(self.count(),2)
        self.assertEqual(self.archive.cursor('demo',SESSION),2)
        self.archive.import_page('demo',page(2,3))
        with self.assertRaises(ValueError):self.archive.import_page('demo',page(lost=[dict(first='2',last='3')]))

    def test_bounds_status_and_gap_overlaps(self):
        for invalid in [page(*range(1,18)),page(1,lost=[dict(first='1',last='1')]),
                        page(lost=[dict(first='1',last='3'),dict(first='3',last='4')])]:
            with self.subTest(page=invalid),self.assertRaises(ValueError):self.archive.import_page('demo',invalid)
        p=page(1);p['records'][0]['status']='timeout'
        with self.assertRaises(ValueError):self.archive.import_page('demo',p)
        p['records'][0]['raw_hex']=''
        self.archive.import_page('demo',p)
        self.assertEqual(self.count(),1)

    def test_oversized_fields_are_rejected_before_serialization(self):
        huge=page(1);huge['extra']='x'*1_000_000
        tracemalloc.start()
        try:
            with self.assertRaises(ValueError):self.archive.import_page('demo',huge)
            _,peak=tracemalloc.get_traced_memory()
        finally:tracemalloc.stop()
        self.assertLess(peak,100_000)
        for key in ('sequence','raw_hex'):
            huge=page(1);huge['records'][0][key]='1'*1_000_000
            with self.assertRaises(ValueError):self.archive.import_page('demo',huge)
        self.assertEqual(self.count(),0)

    def test_two_importers_cannot_move_cursor_backwards(self):
        ready=threading.Event();go=threading.Event();attempted=threading.Event();a_done=threading.Event()
        failures=[]
        def second_importer():
            try:
                archive=LogArchive(self.path)
                archive.db.execute('PRAGMA busy_timeout=0')
                ready.set()
                if not go.wait(2):raise AssertionError('Missing cursor read')
                try:archive.import_page('demo',page(1,2))
                except sqlite3.OperationalError as error:
                    if 'locked' not in str(error):raise
                    attempted.set()
                    if not a_done.wait(2):raise AssertionError('First importer stalled')
                    archive.import_page('demo',page(1,2))
                else:attempted.set()
                archive.close()
            except Exception as error:
                failures.append(error);attempted.set();ready.set()
        original_cursor=self.archive.cursor
        intercepted=False
        def interleaved_cursor(device,session):
            nonlocal intercepted
            value=original_cursor(device,session)
            if not intercepted:
                intercepted=True;go.set()
                if not attempted.wait(2):raise AssertionError('Second importer stalled')
            return value
        thread=threading.Thread(target=second_importer)
        thread.start()
        self.assertTrue(ready.wait(2))
        self.archive.cursor=interleaved_cursor
        try:self.archive.import_page('demo',page(1))
        finally:a_done.set()
        thread.join(2)
        self.assertFalse(thread.is_alive())
        self.assertEqual(failures,[])
        self.assertEqual(original_cursor('demo',SESSION),2)
        self.assertEqual(self.count(),2)


if __name__=='__main__':unittest.main()

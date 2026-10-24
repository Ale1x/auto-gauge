import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'shared/reference'))
from log_import import LogArchive


class SharedLogScenarios(unittest.TestCase):
    def test_shared_pages_with_reload_between_operations(self):
        scenarios=json.loads((ROOT/'shared/fixtures/log.scenarios.json').read_text())
        for scenario in scenarios:
            with self.subTest(scenario=scenario['name']),tempfile.TemporaryDirectory() as temp:
                path=str(Path(temp)/'log.sqlite')
                for operation in scenario['operations']:
                    archive=LogArchive(path)
                    try:
                        device=scenario['device_id'];page=operation['page']
                        if operation['accepted']:
                            ack=archive.import_page(device,page)
                            self.assertEqual(ack['acknowledged_through'],operation['acknowledged_through'])
                        else:
                            with self.assertRaises(ValueError):archive.import_page(device,page)
                        self.assertEqual(str(archive.cursor(device,page['session_id'])),operation['acknowledged_through'])
                        self.assertEqual(archive.db.execute('SELECT count(*) FROM records').fetchone()[0],operation['record_count'])
                    finally:archive.close()

    def test_two_clients_keep_independent_progress_without_reset(self):
        page=json.loads((ROOT/'shared/fixtures/log.scenarios.json').read_text())[0]['operations'][0]['page']
        with tempfile.TemporaryDirectory() as temp:
            a=LogArchive(str(Path(temp)/'phone-a.sqlite'))
            b=LogArchive(str(Path(temp)/'phone-b.sqlite'))
            try:
                a.import_page('same-gauge',page)
                self.assertEqual(b.cursor('same-gauge',page['session_id']),0)
                b.import_page('same-gauge',page)
                self.assertEqual(a.cursor('same-gauge',page['session_id']),2)
                self.assertEqual(b.cursor('same-gauge',page['session_id']),2)
            finally:a.close();b.close()


if __name__=='__main__':unittest.main()

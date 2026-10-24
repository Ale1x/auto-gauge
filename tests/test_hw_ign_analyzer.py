"""Synthetic fixtures test the analyzer only; these are not DUT measurements."""
import csv
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('hw_ign', Path(__file__).resolve().parents[1]/'scripts/analyze_hw_ign.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class AnalyzerTest(unittest.TestCase):
    def capture(self, rows):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/'synthetic.csv'
            with path.open('w', newline='') as f:
                writer = csv.writer(f)
                writer.writerow(module.FIELDS)
                writer.writerows(rows)
            return module.analyze(path)

    def row(self, t, truth='OFF', current=0.00005, requests=0):
        return [t, 12, current, truth, 'SLEEP', requests, requests, 0, 'SYNTHETIC UNIT TEST']

    def test_empty_is_not_pass(self):
        result = self.capture([])
        self.assertEqual(result['capture_status'], 'NO_DATA')
        self.assertEqual(result['verdict'], 'NOT_DETERMINED')

    def test_unknown_or_initial_off_has_no_known_deadline(self):
        result = self.capture([self.row(0), self.row(400), self.row(500,'UNKNOWN'), self.row(900)])
        self.assertEqual(result['samples_after_observed_OFF_plus_300s'], [])

    def test_late_peak_and_requests_are_reported_without_verdict(self):
        result = self.capture([self.row(0,'ON'), self.row(1), self.row(301), self.row(302,current=.002, requests=1)])
        self.assertTrue(result['samples_after_observed_OFF_plus_300s'][-1]['above_100uA'])
        self.assertEqual(result['obd_counter_changes_in_late_OFF_intervals'][0]['obd_requests_delta'],1)
        self.assertEqual(result['verdict'],'NOT_DETERMINED')

    def test_nan_duplicate_time_and_counter_reset_are_rejected(self):
        for rows in ([self.row(0,current='nan')], [self.row(0),self.row(0)],
                     [self.row(0, requests=1),self.row(1,requests=0)]):
            with self.subTest(rows=rows), self.assertRaises(ValueError):
                self.capture(rows)

if __name__ == '__main__':
    unittest.main()

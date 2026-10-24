"""Reject the false-clean summaries produced by the installed MCP parser."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ERCReportGuard(unittest.TestCase):
    def run_checker(self, report):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'scripts').mkdir()
            (root / 'hardware/exports').mkdir(parents=True)
            files = ['scripts/check_review_schematic.py', 'hardware/pin_matrix.csv',
                     'hardware/schematic_bom_review.csv', 'hardware/exports/auto_gauge.net.xml']
            for name in files:
                shutil.copyfile(ROOT / name, root / name)
            (root / 'hardware/exports/erc.json').write_text(json.dumps(report))
            return subprocess.run([sys.executable, str(root / files[0])],
                                  capture_output=True, text=True)

    def test_flattened_empty_mcp_result_is_rejected(self):
        result = self.run_checker({'violations': []})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Missing hierarchical ERC results', result.stderr)

    def test_empty_hierarchical_result_cannot_hide_known_open_circuits(self):
        result = self.run_checker({'sheets': [{'violations': []}]})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('A0 ERC baseline changed', result.stderr)

    def test_real_report_remains_explicitly_open(self):
        result = self.run_checker(json.loads((ROOT / 'hardware/exports/erc.json').read_text()))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)['erc_status'],
                         'OPEN FINDINGS REMAIN; NOT CLEAN / NOT WAIVED')


if __name__ == '__main__':
    unittest.main()

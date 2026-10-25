"""Independent catalog-vs-record oracle after actual simulated journal eviction."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]


class JournalCatalogReviewTests(unittest.TestCase):
    def test_catalog_eviction_and_corruption_under_sanitizers(self):
        with tempfile.TemporaryDirectory(prefix='ag-catalog-review-') as directory:
            binary=Path(directory)/'catalog'
            subprocess.run([shutil.which(os.environ.get('CC','cc')),'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-UNDEBUG',
                '-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(ROOT/'firmware/include'),
                *[str(ROOT/path) for path in ['firmware/src/history.c','firmware/src/journal_history.c','firmware/src/flash_history_sim.c',
                    'firmware/tests/test_journal_catalog_review.c']],'-o',str(binary)],check=True)
            result=subprocess.run([str(binary)],capture_output=True,text=True,timeout=30)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertIn('PASS: 3 independent journal-catalog groups',result.stdout)

"""Independent production boot-health core ordering/fault review."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]


class BootHealthReviewTests(unittest.TestCase):
    def test_independent_ordering_with_sanitizers(self):
        with tempfile.TemporaryDirectory(prefix='ag-boot-review-') as directory:
            binary=Path(directory)/'boot_review'
            subprocess.run([shutil.which(os.environ.get('CC','cc')),'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror',
                '-UNDEBUG','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(ROOT/'firmware/include'),
                str(ROOT/'firmware/src/boot_health.c'),str(ROOT/'firmware/tests/test_boot_health_review.c'),'-o',str(binary)],check=True)
            run=subprocess.run([str(binary)],capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertIn('PASS: 3 independent boot-health groups',run.stdout)

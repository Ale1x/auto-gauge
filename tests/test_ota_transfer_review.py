"""Independent transfer-owner faults with behavioral callbacks, not flash/crypto."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class OTATransferReviewTests(unittest.TestCase):
    def test_independent_owner_faults_with_sanitizers(self):
        compiler = shutil.which(os.environ.get('CC', 'cc'))
        if compiler is None:
            raise RuntimeError('C compiler required')
        with tempfile.TemporaryDirectory(prefix='ag-transfer-review-') as directory:
            executable = Path(directory) / 'review'
            subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                            '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-UNDEBUG',
                            '-I', str(ROOT/'firmware/include'),
                            str(ROOT/'firmware/src/ota_transfer.c'),
                            str(ROOT/'firmware/tests/test_ota_transfer_review.c'),
                            '-o', str(executable)], check=True)
            result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(result.stdout.count('PASS:'), 3)

"""Independent production framing/owner tests with behavioral callbacks only."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]


class OTAFramedReviewTests(unittest.TestCase):
    def test_independent_fault_groups(self):
        with tempfile.TemporaryDirectory(prefix='ag-framed-review-') as directory:
            binary=Path(directory)/'review'
            command=[shutil.which(os.environ.get('CC','cc')),'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror',
                     '-UNDEBUG','-fsanitize=address,undefined','-fno-omit-frame-pointer',
                     '-I',str(ROOT/'firmware/include'),'-I',str(ROOT/'shared/c')]
            command += [str(ROOT/path) for path in ['firmware/src/ota_transfer.c','firmware/src/ota_framed_endpoint.c',
                'shared/c/ag_frame.c','shared/c/ag_stream.c','firmware/tests/test_ota_framed_review.c']]
            subprocess.run(command+['-o',str(binary)],check=True)
            result=subprocess.run([str(binary)],capture_output=True,text=True,timeout=30)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertEqual(result.stdout.count('PASS: reviewer'),3,result.stdout)

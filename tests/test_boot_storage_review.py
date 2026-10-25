"""Production boot storage/core with behavioral NVS/SDK and NONCRYPTO hash."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]


class BootStorageReviewTests(unittest.TestCase):
    def test_binding_under_sanitizers(self):
        with tempfile.TemporaryDirectory(prefix='ag-boot-binding-') as directory:
            binary=Path(directory)/'boot_storage_review'
            command=[shutil.which(os.environ.get('CC','cc')),'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-UNDEBUG',
                     '-fsanitize=address,undefined','-fno-omit-frame-pointer']
            for path in ['firmware/ota_study/tests/boot_shim','firmware/ota_study/main','firmware/include']:
                command+=['-I',str(ROOT/path)]
            command += [str(ROOT/path) for path in ['firmware/src/boot_health.c','firmware/ota_study/main/boot_storage.c',
                'firmware/ota_study/tests/boot_storage_review_driver.c']]
            subprocess.run(command+['-o',str(binary)],check=True)
            run=subprocess.run([str(binary)],capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertIn('PASS: 4 boot-storage binding groups',run.stdout)

"""Real C transfer/adapter/installer with explicit noncrypto SDK doubles."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]


class OTATransferAdapterTests(unittest.TestCase):
    def test_bound_installer_with_sanitizers(self):
        with tempfile.TemporaryDirectory(prefix='ag-ota-adapter-') as directory:
            binary=Path(directory)/'adapter'
            command=[shutil.which(os.environ.get('CC','cc')), '-std=c11','-Wall','-Wextra','-Wpedantic','-Werror',
                     '-UNDEBUG','-fsanitize=address,undefined','-fno-omit-frame-pointer']
            for path in ['firmware/ota_study/tests/shim','firmware/ota_study/main','firmware/include']:
                command += ['-I',str(ROOT/path)]
            command += [str(ROOT/path) for path in ['firmware/src/ota_transfer.c','firmware/ota_study/main/ota_installer.c',
                        'firmware/ota_study/main/transfer_adapter.c','firmware/ota_study/tests/transfer_adapter_driver.c']]
            subprocess.run(command+['-o',str(binary)],check=True)
            run=subprocess.run([str(binary)],capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertIn('PASS: OTA transfer adapter binding',run.stdout)

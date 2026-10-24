"""Framed endpoint through real adapter/installer with behavioral SDK/crypto."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]


class OTAFramedAdapterTests(unittest.TestCase):
    def test_framed_binding_with_sanitizers(self):
        with tempfile.TemporaryDirectory(prefix='ag-ota-framed-adapter-') as directory:
            binary=Path(directory)/'adapter'
            cmd=[shutil.which(os.environ.get('CC','cc')),'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-UNDEBUG',
                 '-fsanitize=address,undefined','-fno-omit-frame-pointer']
            for p in ['firmware/include','shared/c','firmware/ota_study/main','firmware/ota_study/tests/shim']:cmd+=['-I',str(ROOT/p)]
            cmd += [str(ROOT/p) for p in ['firmware/src/ota_transfer.c','firmware/src/ota_framed_endpoint.c','shared/c/ag_frame.c','shared/c/ag_stream.c',
                'firmware/ota_study/main/ota_installer.c','firmware/ota_study/main/transfer_adapter.c','firmware/ota_study/main/framed_adapter.c',
                'firmware/ota_study/tests/framed_adapter_driver.c']]
            subprocess.run(cmd+['-o',str(binary)],check=True)
            result=subprocess.run([str(binary)],capture_output=True,text=True,timeout=30)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertIn('PASS: framed SDK binding lifecycle',result.stdout)

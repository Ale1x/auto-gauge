"""Installer first-OTA path is an explicit local maintenance capability."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]


class OTAFactoryBootstrapTests(unittest.TestCase):
    def test_explicit_factory_profile_with_sanitizers(self):
        with tempfile.TemporaryDirectory(prefix='ag-factory-test-') as directory:
            binary=Path(directory)/'factory'
            cmd=[shutil.which(os.environ.get('CC','cc')),'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-UNDEBUG','-fsanitize=address,undefined','-fno-omit-frame-pointer',
                 '-I',str(ROOT/'firmware/ota_study/tests/shim'),'-I',str(ROOT/'firmware/ota_study/main'),
                 str(ROOT/'firmware/ota_study/main/ota_installer.c'),str(ROOT/'firmware/ota_study/tests/factory_bootstrap_driver.c'),'-o',str(binary)]
            subprocess.run(cmd,check=True)
            run=subprocess.run([str(binary)],capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertIn('PASS: explicit factory bootstrap',run.stdout)

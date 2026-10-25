"""Actual partition adapter + cores with behavioral SDK partition/flash doubles."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]


class PartitionIOTests(unittest.TestCase):
    def test_adapter_and_cores_under_sanitizers(self):
        with tempfile.TemporaryDirectory(prefix='ag-partition-io-') as directory:
            binary=Path(directory)/'partition_io'
            command=[shutil.which(os.environ.get('CC','cc')),'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-UNDEBUG',
                     '-fsanitize=address,undefined','-fno-omit-frame-pointer']
            for path in ['firmware/platform_study/tests/shim','firmware/platform_study/main','firmware/include']:
                command+=['-I',str(ROOT/path)]
            command += [str(ROOT/path) for path in ['firmware/platform_study/main/partition_io.c',
                'firmware/platform_study/tests/partition_driver.c','firmware/src/config.c','firmware/src/config_store.c',
                'firmware/src/history.c','firmware/src/journal_history.c']]
            subprocess.run(command+['-o',str(binary)],check=True)
            run=subprocess.run([str(binary)],capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertIn('PASS: 4 partition adapter groups',run.stdout)

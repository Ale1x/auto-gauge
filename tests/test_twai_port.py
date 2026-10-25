"""Modern SDK TWAI wrapper + real discovery/runtime; behavioral SDK/timer/queue."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]


class TWAIPortTests(unittest.TestCase):
    def test_core_binding_and_faults_with_sanitizers(self):
        with tempfile.TemporaryDirectory(prefix='ag-twai-port-') as directory:
            binary=Path(directory)/'twai'
            cmd=[shutil.which(os.environ.get('CC','cc')),'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-UNDEBUG',
                 '-fsanitize=address,undefined','-fno-omit-frame-pointer']
            for p in ['firmware/platform_study/tests/twai_shim','firmware/platform_study/main','firmware/include']:cmd+=['-I',str(ROOT/p)]
            cmd += [str(ROOT/p) for p in ['firmware/platform_study/main/twai_port.c','firmware/platform_study/tests/twai_driver.c',
                'firmware/src/config.c','firmware/src/can_policy.c','firmware/src/discovery.c','firmware/src/obd_decode.c','firmware/src/isotp_rx.c',
                'firmware/src/scheduler.c','firmware/src/runtime.c','firmware/src/journal_history.c','firmware/src/history.c']]
            subprocess.run(cmd+['-o',str(binary)],check=True)
            result=subprocess.run([str(binary)],capture_output=True,text=True,timeout=30)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertIn('PASS: 9 modern TWAI port groups',result.stdout)

"""Strict sessions RPC + production endpoint authorization with a host catalog."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]


class HistorySessionsTests(unittest.TestCase):
    def test_rpc_catalog_endpoint_under_sanitizers(self):
        with tempfile.TemporaryDirectory(prefix='ag-history-sessions-') as directory:
            scratch=Path(directory);binary=scratch/'sessions'
            subprocess.run([sys.executable,str(ROOT/'firmware/tools/generate_fixture.py'),
                str(ROOT/'shared/fixtures/config.valid.json'),str(ROOT/'shared/fixtures/config.cases.json'),str(scratch/'shared_fixture.h')],check=True)
            command=[shutil.which(os.environ.get('CC','cc')),'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-UNDEBUG',
                     '-fsanitize=address,undefined','-fno-omit-frame-pointer']
            for path in [scratch,ROOT/'firmware/include',ROOT/'shared/c']:command+=['-I',str(path)]
            command += [str(ROOT/path) for path in ['firmware/src/config.c','firmware/src/can_policy.c','firmware/src/connection.c',
                'firmware/src/admission.c','firmware/src/config_store.c','firmware/src/flash_sim.c','firmware/src/history.c',
                'firmware/src/device_endpoint.c','shared/c/ag_config_json.c','shared/c/ag_frame.c','shared/c/ag_stream.c','shared/c/ag_rpc.c',
                'firmware/tests/test_history_sessions.c']]
            subprocess.run(command+['-o',str(binary)],check=True)
            run=subprocess.run([str(binary)],capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertIn('PASS: 4 history-session catalog groups',run.stdout)

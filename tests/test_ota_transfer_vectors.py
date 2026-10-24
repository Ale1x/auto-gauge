"""Independent public wire vectors against production C; Swift consumes same JSON."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
SPEC=importlib.util.spec_from_file_location('ota_transfer_vectors',ROOT/'scripts/generate_ota_transfer_vectors.py')
VECTORS=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(VECTORS)


class OTATransferVectorsTests(unittest.TestCase):
    def test_vectors_fresh(self):
        self.assertEqual(json.loads((ROOT/'shared/fixtures/ota-transfer.cases.json').read_text()),VECTORS.generate_vectors())

    def test_c_codec_matches_public_vectors(self):
        vectors=VECTORS.generate_vectors()
        with tempfile.TemporaryDirectory(prefix='ag-ota-wire-') as directory:
            binary=Path(directory)/'codec'
            subprocess.run([shutil.which(os.environ.get('CC','cc')),'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror',
                            '-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(ROOT/'firmware/include'),
                            str(ROOT/'firmware/src/ota_transfer.c'),str(ROOT/'firmware/tools/ota_transfer_codec_cli.c'),
                            '-o',str(binary)],check=True)
            for kind in ['request','response']:
                for case in vectors[kind+'s']:
                    with self.subTest(kind=kind,name=case['name']):
                        run=subprocess.run([str(binary),kind,case['hex']],capture_output=True,text=True,timeout=10)
                        self.assertEqual(run.returncode,0,run.stderr)
                        if not case['valid']:expected='INVALID'
                        elif kind=='request':expected=f"{case['opcode']} {case['transfer_id']} {case['offset']} {case['body_hex']}".strip()
                        else:expected=' '.join(str(case[k]) for k in ['status','phase','flags','transfer_id','manifest_offset','image_offset'])
                        self.assertEqual(run.stdout.strip(),expected)

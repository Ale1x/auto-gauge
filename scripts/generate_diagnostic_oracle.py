#!/usr/bin/env python3
"""Generate/check numeric fixtures by running the actual C Service01 decoder."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'shared/fixtures/diagnostic-scalars.cases.json'


def generate():
    compiler=shutil.which(os.environ.get('CC','cc'))
    if compiler is None: raise RuntimeError('C compiler required')
    with tempfile.TemporaryDirectory(prefix='ag-diagnostic-oracle-') as directory:
        executable=Path(directory)/'oracle'
        sources=['firmware/tools/diagnostic_oracle.c','firmware/src/obd_decode.c',
                 'firmware/src/can_policy.c','firmware/src/config.c']
        subprocess.run([compiler,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-UNDEBUG',
            '-fsanitize=address,undefined','-fno-omit-frame-pointer',
            '-I',str(ROOT/'firmware/include'),*[str(ROOT/p) for p in sources],'-o',str(executable)],check=True)
        data=json.loads(subprocess.check_output([str(executable)],timeout=30))
    assert data['pid_count']==53 and len(data['cases'])==212
    return (json.dumps(data,indent=2,ensure_ascii=True,allow_nan=False)+'\n').encode()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    content=generate()
    if args.check:
        if FIXTURE.read_bytes()!=content: raise SystemExit('C decoder oracle fixture is stale')
        print('PASS:53 PIDs/212 vectors match the sanitized C decoder')
    else:
        FIXTURE.write_bytes(content)
        print(FIXTURE)


if __name__=='__main__': main()

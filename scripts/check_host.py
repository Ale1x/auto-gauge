#!/usr/bin/env python3
"""Portable Python/C checks, without requiring Xcode or a device SDK."""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]


def run(args):
    print('+ '+' '.join(map(str,args)),flush=True)
    subprocess.run(list(map(str,args)),cwd=ROOT,check=True)


def check_host(build):
    run([sys.executable,'scripts/check_shared_contract.py'])
    run([sys.executable,'-m','unittest','discover','-s','tests','-v'])
    run(['cmake','-S',ROOT/'firmware','-B',build,'-DCMAKE_BUILD_TYPE=Debug'])
    run(['cmake','--build',build])
    run(['ctest','--test-dir',build,'--output-on-failure'])


if __name__=='__main__':
    with tempfile.TemporaryDirectory(prefix='auto-gauge-host-') as temp:
        check_host(Path(temp)/'firmware')
    print('Host checks passed; no device, radio, cloud or automotive validation performed.')

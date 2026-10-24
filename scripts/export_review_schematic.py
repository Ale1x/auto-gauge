#!/usr/bin/env python3
"""Export and verify the schematic review bundle. Does not export PCB production files."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import zipfile

ROOT=Path(__file__).resolve().parents[1]
HW=ROOT/'hardware';OUT=HW/'exports'
CLI='/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli'
OUT.mkdir(exist_ok=True)
def python(name):subprocess.run([sys.executable,str(ROOT/'scripts'/name)],cwd=ROOT,check=True)
def command(name,args):
    result=subprocess.run([CLI,*args],cwd=ROOT,text=True,capture_output=True)
    (OUT/(name+'-cli.log')).write_text(result.stdout+'\n'+result.stderr)
    result.check_returncode()
    print(name+': export completed')
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--bundle-only',action='store_true',help='Recheck and package existing exports without changing schematic or review hashes')
args=parser.parse_args()
if not args.bundle_only:
    python('build_buck5_screen.py')
    python('generate_review_schematic.py')
    schematic=str(HW/'auto_gauge.kicad_sch')
    command('netlist',['sch','export','netlist','--format','kicadxml','--output',str(OUT/'auto_gauge.net.xml'),schematic])
    command('erc',['sch','erc','--format','json','--output',str(OUT/'erc.json'),schematic])
    command('pdf',['sch','export','pdf','--output',str(OUT/'auto_gauge_A0.pdf'),schematic])
    command('svg',['sch','export','svg','--output',str(OUT/'svg')+'/',schematic])
python('check_review_schematic.py')
files=sorted([*HW.glob('*.kicad_sch'),HW/'auto_gauge.kicad_pro',HW/'AutoGauge.kicad_sym',HW/'sym-lib-table',HW/'README.md',HW/'ERC_REVIEW.md',HW/'pin_matrix.csv',HW/'schematic_bom_review.csv',OUT/'auto_gauge_A0.pdf',OUT/'auto_gauge.net.xml',OUT/'erc.json',OUT/'connectivity_audit.json',*sorted((OUT/'svg').glob('*.svg')),*sorted((HW/'reviews').glob('*.md'))])
manifest={str(p.relative_to(HW)):dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in files}
support=[ROOT/'BUCK5_FEEDBACK_REVIEW.md',ROOT/'evidence/models/buck5_ripple_screen.csv',ROOT/'evidence/models/buck5_ripple_screen.json',ROOT/'scripts/build_buck5_screen.py']
for p in support:
    manifest['analysis/'+p.name]=dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest())
manifest_path=OUT/'review_bundle_manifest.json'
manifest_path.write_text(json.dumps(dict(revision='A0',status='INCOMPLETE ENGINEERING REVIEW / OPEN BLOCKERS',files=manifest),indent=2)+'\n')
archive=OUT/'auto_gauge_A0_review.zip'
with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED) as z:
    for p in [*files,manifest_path]:z.write(p,'auto_gauge_A0/'+str(p.relative_to(HW)))
    for p in support:z.write(p,'auto_gauge_A0/analysis/'+p.name)
print(f'Wrote review archive with {len(files)+len(support)+1} files: {archive}')

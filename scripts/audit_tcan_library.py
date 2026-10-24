#!/usr/bin/env python3
"""Read-only KiCad library comparison against E03, not a release approval."""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = dict(enumerate(['TXD','GND','VCC','RXD','VIO','EN','INH','nFAULT','WAKE','VSUP','NC','CANL','CANH','nSTB'], 1))

def parse(text):
    tokens = iter(re.findall(r'\(|\)|"(?:\\.|[^"\\])*"|[^\s()]+',text))
    def node(first):
        if first != '(':
            return json.loads(first) if first.startswith('"') else first
        items = []
        for token in tokens:
            if token == ')':
                return items
            items.append(node(token))
        raise ValueError('unterminated expression')
    return node(next(tokens))

def children(tree, tag):
    return [x for x in tree if isinstance(x,list) and x and x[0]==tag]

def descendants(tree, tag):
    for item in tree:
        if isinstance(item,list):
            if item and item[0]==tag:
                yield item
            yield from descendants(item,tag)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library-root',type=Path,default=Path('/Applications/KiCad/KiCad.app/Contents/SharedSupport'))
    args = parser.parse_args()
    source = args.library_root/'symbols/Interface_CAN_LIN.kicad_sym'
    footprint = args.library_root/'footprints/Package_SO.pretty/SOIC-14_3.9x8.7mm_P1.27mm.kicad_mod'
    library = parse(source.read_text())
    symbol = next(x for x in children(library,'symbol') if x[1]=='TCAN1043xDxQ1')
    pins = []
    for pin in descendants(symbol,'pin'):
        name = children(pin,'name')[0][1]
        number = int(children(pin,'number')[0][1])
        normalized = 'n'+name[2:-1] if name.startswith('~{') and name.endswith('}') else name
        pins.append(dict(number=number, name=name, normalized_name=normalized,
                         electrical_type=pin[1], expected=EXPECTED.get(number),
                         name_matches=normalized==EXPECTED.get(number)))
    pins.sort(key=lambda x:x['number'])
    pads=[]
    for pad in children(parse(footprint.read_text()),'pad'):
        pads.append(dict(number=pad[1], at_mm=list(map(float,children(pad,'at')[0][1:3])),
                         size_mm=list(map(float,children(pad,'size')[0][1:3]))))
    report = dict(scope='TCAN1043ADRQ1 SOIC-D only; pin-name comparison to manually transcribed E03 section4 table; no design/footprint approval',
                  symbol_name='Interface_CAN_LIN:TCAN1043xDxQ1',
                  files={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (source,footprint,ROOT/'evidence/tcan1043a-q1.pdf')},
                  symbol_datasheet=next(x[2] for x in children(symbol,'property') if x[1]=='Datasheet'),
                  pins=pins, pads=pads,
                  all_14_pin_names_match=len(pins)==14 and all(x['name_matches'] for x in pins) and {x['number'] for x in pins}==set(EXPECTED),
                  pad_numbers_1_to_14=sorted(int(x['number']) for x in pads)==list(range(1,15)),
                  disposition='NOT_FROZEN: generic symbol points to non-A datasheet; land geometry differs from TI example; high-voltage roles and ERC semantics need review')
    out=ROOT/'evidence/kicad-tcan-library-audit.json'
    out.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:report[k] for k in ('all_14_pin_names_match','pad_numbers_1_to_14','disposition')}))

if __name__=='__main__':
    main()

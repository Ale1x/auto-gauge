#!/usr/bin/env python3
"""Deterministic public wire-format vectors; no keys, radio or flash."""
import argparse
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]


def generate_vectors():
    requests = []
    responses = []
    def request(name, opcode, transfer_id=0x12345678, offset=0, body=b'', valid=True, raw=None):
        value = struct.pack('<BBHII', 1, opcode, 0, transfer_id, offset) + body if raw is None else raw
        item = dict(name=name, hex=value.hex(), valid=valid)
        if valid:
            item.update(opcode=opcode, transfer_id=transfer_id, offset=offset, body_hex=body.hex())
        requests.append(item)
        return value
    start=request('start_min',1,offset=1)
    request('start_max',1,offset=4096)
    request('manifest_binary',2,offset=0,body=bytes([0,1,255,128]))
    request('manifest_last',2,offset=4095,body=b'x')
    request('manifest_max_chunk',2,body=bytes(range(256))*4)
    for opcode,name in [(3,'begin'),(5,'finish'),(6,'select'),(7,'cancel')]:
        request(name,opcode)
    request('image_binary',4,offset=1024,body=bytes(range(32)))
    request('image_last',4,offset=16*1024*1024-1,body=b'x')
    for n in range(12):request('truncated_header_'+str(n),1,valid=False,raw=start[:n])
    for name,raw in [('version',bytes([2])+start[1:]),('reserved',start[:2]+b'\x01\x00'+start[4:]),('opcode_zero',start[:1]+b'\x00'+start[2:]),('opcode_eight',start[:1]+b'\x08'+start[2:])]:
        request(name,1,valid=False,raw=raw)
    for name,op,off,body,tid in [('zero_id',1,1,b'',0),('start_zero',1,0,b'',1),('start_over',1,4097,b'',1),('start_body',1,1,b'x',1),('manifest_empty',2,0,b'',1),('manifest_over',2,4096,b'x',1),('image_empty',4,0,b'',1),('image_over',4,16*1024*1024,b'x',1),('image_uintmax',4,0xffffffff,b'x',1),('chunk_over',4,0,b'x'*1025,1),('begin_offset',3,1,b'',1),('finish_body',5,0,b'x',1),('select_body',6,0,b'x',1),('cancel_offset',7,1,b'',1)]:
        request(name,op,transfer_id=tid,offset=off,body=body,valid=False)
    def response(name,status=0,phase=1,flags=0,transfer_id=0x12345678,manifest_offset=4096,image_offset=0,valid=True,raw=None):
        value=struct.pack('<BBBBIII',1,status,phase,flags,transfer_id,manifest_offset,image_offset) if raw is None else raw
        item=dict(name=name,hex=value.hex(),valid=valid)
        if valid:item.update(status=status,phase=phase,flags=flags,transfer_id=transfer_id,manifest_offset=manifest_offset,image_offset=image_offset)
        responses.append(item)
        return value
    good=response('manifest_ack')
    response('idle_denied',status=2,phase=0,transfer_id=0,manifest_offset=0)
    response('writing',phase=2,image_offset=1024)
    response('ready',phase=3,image_offset=16*1024*1024)
    response('selected',phase=4,flags=1,image_offset=2080)
    response('select_fault',status=4,phase=5,flags=1,image_offset=2080)
    response('busy',status=5)
    response('expired',status=3,phase=5)
    response('invalid',status=1)
    for n in [0,1,12,15]:response('truncated_'+str(n),valid=False,raw=good[:n])
    response('trailing',valid=False,raw=good+b'x')
    response('bad_version',valid=False,raw=b'\x02'+good[1:])
    response('bad_status',status=6,valid=False)
    response('bad_phase',phase=6,valid=False)
    response('bad_flags',flags=2,valid=False)
    response('manifest_over',manifest_offset=4097,valid=False)
    response('image_over',image_offset=16*1024*1024+1,valid=False)
    return dict(schema_version=1,requests=requests,responses=responses)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    path=ROOT/'shared/fixtures/ota-transfer.cases.json'
    expected=json.dumps(generate_vectors(),indent=2)+'\n'
    if args.check:
        if path.read_text()!=expected:raise SystemExit('Stale OTA transfer vectors')
        print('PASS: OTA transfer vectors current')
    else:path.write_text(expected)


if __name__=='__main__':main()

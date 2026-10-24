#!/usr/bin/env python3
"""Host fixture only: binary C endpoint vs original shared exchanges. No radio."""
import argparse
import copy
import json
import os
from pathlib import Path
import select
import subprocess
import sys
import tempfile
import time
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'shared/reference'))
from framing import Frame, encode, decode, HEADER

class Peer:
    def __init__(self, executable, root, store=None, seed=True):
        self.control=root/'control.txt';self.control.write_text('')
        args=[str(executable),'--trusted-fixture-admission','--control-file',str(self.control)]
        if store is not None:args+=['--store-file',str(store)]
        if seed:args+=['--seed-initial']
        self.process=subprocess.Popen(args,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    def command(self, text):self.control.write_text(text+'\n')
    def send(self, request_id, request):
        payload=json.dumps(request,sort_keys=True,separators=(',',':')).encode()
        packet=encode(Frame(1,request_id,payload));self.process.stdin.write(packet);self.process.stdin.flush()
    def exact(self,n,timeout=3):
        deadline=time.monotonic()+timeout;data=bytearray()
        while len(data)<n:
            remain=deadline-time.monotonic()
            if remain<=0 or not select.select([self.process.stdout],[],[],remain)[0]:raise AssertionError('endpoint response deadline')
            chunk=os.read(self.process.stdout.fileno(),n-len(data))
            if not chunk:raise AssertionError('endpoint closed unexpectedly')
            data+=chunk
        return bytes(data)
    def reply(self,request_id):
        header=self.exact(12);length=HEADER.unpack(header)[4]
        frame=decode(header+self.exact(length+4));assert frame.kind==2 and frame.request_id==request_id
        return json.loads(frame.payload)
    def request(self,request_id,request):self.send(request_id,request);return self.reply(request_id)
    def close(self,kill=False):
        if self.process.poll() is None:
            if kill:self.process.terminate()
            else:self.process.stdin.close()
        try:self.process.wait(timeout=3)
        except subprocess.TimeoutExpired:self.process.kill();self.process.wait(timeout=3)
        stderr=self.process.stderr.read();assert not stderr,'unexpected endpoint diagnostic output'
        if not kill:assert self.process.returncode==0

def expected_response(value,durable):
    result=copy.deepcopy(value)
    if result.get('op')=='hello':result['authenticated']=True  # Explicit trusted test hook, never radio qualification.
    if durable and result.get('persistence')=='volatile':result['persistence']='durable'
    return result

def apply_request(configuration,revision,expected):
    config=copy.deepcopy(configuration);config['revision']=revision
    return dict(op='config.apply',configuration=config,expected_revision=expected)

def run(executable):
    fixture=json.loads((ROOT/'shared/fixtures/rpc.exchanges.json').read_text())
    with tempfile.TemporaryDirectory(prefix='ag-endpoint-interop-') as temporary:
        root=Path(temporary);store=root/'configuration.bin'
        for durable in (False,True):
            peer=Peer(executable,root,store if durable else None)
            try:
                for exchange in fixture['exchanges']:
                    peer.send(exchange['request_id'],exchange['request'])
                    for response in exchange['responses']:
                        assert peer.reply(exchange['request_id'])==expected_response(response,durable),('golden exchange',exchange['request_id'])
                # B imports after A's ACK; then A reconnects with a new lease, ID sequence and no reset.
                for client in ('connect-b','connect-a'):
                    peer.command(client);assert peer.request(1,dict(op='hello'))['simulation'] is True
                    assert peer.request(2,fixture['exchanges'][6]['request'])==expected_response(fixture['exchanges'][6]['responses'][0],durable)
                peer.command('new-log-session')
                new=copy.deepcopy(fixture['exchanges'][6]['request']);new['session_id']='06'*16
                reply=peer.request(3,new);assert reply['page']['session_id']=='06'*16 and reply['page']['records'][0]['sequence']=='3'
            finally:peer.close()
        assert store.stat().st_size==8192
        # Process restart restores revision2 without implicit seeding/provisioning.
        peer=Peer(executable,root,store,seed=False)
        try:
            assert peer.request(1,dict(op='config.get'))['configuration']['revision']==2
            peer.command('drop-applied');peer.send(2,apply_request(fixture['initial_configuration'],3,2))
            assert peer.reply(2)['phase']=='received'
            assert not select.select([peer.process.stdout],[],[],0.1)[0],'lost-ACK fixture unexpectedly emitted applied'
        finally:peer.close(kill=True)
        peer=Peer(executable,root,store,seed=False)
        try:
            current=peer.request(1,dict(op='config.get'));assert current['persistence']=='durable' and current['configuration']['revision']==3
            peer.command('fail-after-marker');peer.send(2,apply_request(fixture['initial_configuration'],4,3))
            assert peer.reply(2)['phase']=='received'
            unknown=peer.reply(2);assert unknown['phase']=='outcome_unknown' and unknown['error']=='outcome_unknown'
        finally:peer.close(kill=True)
        peer=Peer(executable,root,store,seed=False)
        try:assert peer.request(1,dict(op='config.get'))['configuration']['revision']==4
        finally:peer.close()
        # Newest corrupt slot: expose diagnostic status, never current/durable from fallback.
        data=bytearray(store.read_bytes());active=max((0,4096),key=lambda at:int.from_bytes(data[at+8:at+16],'little'))
        data[active]^=1;store.write_bytes(data)
        peer=Peer(executable,root,store,seed=False)
        try:
            diagnostic=peer.request(1,dict(op='config.get'))
            assert diagnostic['phase']=='diagnostic' and diagnostic['store_state']=='read_only' and 'persistence' not in diagnostic
        finally:peer.close()
        data[4096-active]^=1;store.write_bytes(data);before=store.read_bytes()
        rejected=subprocess.run([str(executable),'--trusted-fixture-admission','--store-file',str(store),'--seed-initial'],input=b'',capture_output=True,timeout=3)
        assert rejected.returncode==3 and rejected.stdout==b'' and rejected.stderr==b'' and store.read_bytes()==before
        no_fixture=subprocess.run([str(executable),'--seed-initial'],input=b'',capture_output=True,timeout=3)
        assert no_fixture.returncode==2 and no_fixture.stdout==b'' and no_fixture.stderr==b''
    print('PASS: C endpoint binary vs20 original exchanges (RAM/file), A-B-A, ACK retention, newSID, fsync restore, lostACK, post-marker IO ambiguity, readonly/no-default')
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--executable',type=Path,required=True)
    run(parser.parse_args().executable.resolve())

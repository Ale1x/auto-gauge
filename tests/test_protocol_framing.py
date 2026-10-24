import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'shared/reference'))
from framing import Frame, FrameError, MAX_FRAME, StreamDecoder, decode, encode

CASES = json.loads((ROOT/'shared/fixtures/frame.cases.json').read_text())


class FramingTests(unittest.TestCase):
    def test_golden_packets(self):
        for case in CASES:
            with self.subTest(case=case['name']):
                packet = bytes.fromhex(case['frame_hex'])
                if not case['valid']:
                    with self.assertRaises(FrameError): decode(packet)
                else:
                    frame = Frame(case['kind'],case['request_id'],bytes.fromhex(case['payload_hex']))
                    self.assertEqual(decode(packet),frame)
                    self.assertEqual(encode(frame),packet)

    def test_every_split_and_multiple_packets(self):
        frame = Frame(1,42,b'{"op":"hello"}')
        packet = encode(frame)
        for split in range(len(packet)+1):
            decoder=StreamDecoder()
            self.assertEqual(decoder.feed(packet[:split],0)+decoder.feed(packet[split:],1),[frame])
        self.assertEqual(StreamDecoder().feed(packet*3,0),[frame]*3)

    def test_max_payload_and_stream_memory_bound(self):
        frame=Frame(1,1,b'x'*4096)
        packet=encode(frame)
        self.assertEqual(len(packet),MAX_FRAME)
        decoder=StreamDecoder(); result=[]
        for i in range(0,len(packet),20):
            result.extend(decoder.feed(packet[i:i+20],i//20))
            self.assertLessEqual(len(decoder.buffer),MAX_FRAME)
        self.assertEqual(result,[frame])
        with self.assertRaises(FrameError):encode(Frame(1,1,b'x'*4097))

    def test_timeout_is_not_extended_by_fragment_arrival(self):
        decoder=StreamDecoder(); packet=encode(Frame(1,1,b'abc'))
        self.assertEqual(decoder.feed(packet[:8],0),[])
        self.assertEqual(decoder.feed(packet[8:12],999),[])
        with self.assertRaises(FrameError):decoder.feed(b'',1000)
        self.assertEqual(len(decoder.buffer),0)
        self.assertEqual(decoder.feed(packet,1001),[Frame(1,1,b'abc')])

    def test_crc_clock_and_chunk_errors_reset_partial_state(self):
        packet=bytearray(encode(Frame(1,1,b'abc')));packet[-1]^=1
        decoder=StreamDecoder()
        with self.assertRaises(FrameError):decoder.feed(packet,5)
        self.assertEqual(len(decoder.buffer),0)
        decoder.feed(b'A',6)
        with self.assertRaises(FrameError):decoder.feed(b'',4)
        self.assertEqual(len(decoder.buffer),0)
        with self.assertRaises(FrameError):decoder.feed(b'x'*513,7)


class CInteropTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc=shutil.which('cc')
        if cc is None: raise unittest.SkipTest('C compiler unavailable; C interop not verified')
        cls.temp=tempfile.TemporaryDirectory(prefix='ag-frame-test-')
        cls.binary=Path(cls.temp.name)/'frame_codec'
        subprocess.run([cc,'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                        '-I',str(ROOT/'shared/c'),str(ROOT/'shared/c/ag_frame.c'),
                        str(ROOT/'tests/frame_codec_driver.c'),'-o',str(cls.binary)],check=True)

    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()

    def invoke(self,*args):
        return subprocess.run([str(self.binary),*map(str,args)],capture_output=True,text=True)

    def test_c_invalid_boundaries(self): self.assertEqual(self.invoke('selftest').returncode,0)

    def test_c_python_same_golden_bytes(self):
        for case in CASES:
            with self.subTest(case=case['name']):
                result=self.invoke('decode',case['frame_hex'])
                if not case['valid']:
                    self.assertEqual(result.returncode,2,result.stderr)
                    continue
                self.assertEqual(result.returncode,0,result.stderr)
                parts=result.stdout.rstrip('\n').split(' ',2)
                self.assertEqual(parts,[str(case['kind']),str(case['request_id']),case['payload_hex']])
                result=self.invoke('encode',case['kind'],case['request_id'],case['payload_hex'])
                self.assertEqual(result.returncode,0,result.stderr)
                self.assertEqual(result.stdout.strip(),case['frame_hex'])

    def test_maximum_payload_in_c_both_directions(self):
        frame=Frame(1,123,b'\xaa'*4096)
        packet=encode(frame)
        result=self.invoke('encode',1,123,frame.payload.hex())
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(result.stdout.strip(),packet.hex())
        self.assertEqual(self.invoke('decode',packet.hex()).returncode,0)
        self.assertEqual(self.invoke('encode',1,123,(frame.payload+b'x').hex()).returncode,2)


if __name__=='__main__':unittest.main()

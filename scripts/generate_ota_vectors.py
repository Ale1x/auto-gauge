#!/usr/bin/env python3
"""Ephemeral shared signed OTA vectors. Private keys never leave a temporary directory."""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'shared/reference'))
from ota_manifest import ALGORITHM,DOMAIN,PackageError,signing_bytes,verify_package


def _b64(data):return base64.b64encode(data).decode('ascii')


def _image():
    header=bytearray(24);header[0:4]=bytes((0xe9,1,2,0x20));header[23]=1
    struct.pack_into('<I',header,4,0x40370000);struct.pack_into('<H',header,12,9)
    struct.pack_into('<H',header,17,99)
    descriptor=bytearray(256);struct.pack_into('<II',descriptor,0,0xabcd5432,1)
    descriptor[16:22]=b'1.0.0\0';descriptor[48:57]=b'TEST-ONLY'
    body=header+struct.pack('<II',0x3c000020,len(descriptor))+descriptor
    checksum=0xef
    for value in descriptor:checksum^=value
    body+=bytes(((len(body)+16)//16)*16-len(body)-1)+bytes((checksum,))
    return bytes(body+hashlib.sha256(body).digest())


def generate_vectors():
    executable=shutil.which('openssl')
    if executable is None:raise RuntimeError('OpenSSL required')
    with tempfile.TemporaryDirectory(prefix='ag-ota-cross-vectors-') as directory:
        root=Path(directory)
        def run(*arguments):
            return subprocess.run([executable,*map(str,arguments)],capture_output=True,check=True,timeout=30).stdout
        def key(bits,name):
            private=root/(name+'-private.pem');public=root/(name+'-public.pem');der=root/(name+'-public.der')
            run('genpkey','-algorithm','RSA','-pkeyopt',f'rsa_keygen_bits:{bits}','-out',private)
            run('pkey','-in',private,'-pubout','-out',public)
            run('rsa','-in',private,'-RSAPublicKey_out','-outform','DER','-out',der)
            return private,public,der.read_bytes()
        private,public,der=key(3072,'main')
        private_small,public_small,der_small=key(3070,'small')
        image=_image();key_id='ephemeral-test';hardware='TEST-ONLY-NOT-A0';floor='9007199254740992'
        base=dict(schema_version=1,target='esp32s3',hardware_id=hardware,firmware_version='1.0.0',
            release_counter='9007199254740993',image_bytes=len(image),
            image_sha256=hashlib.sha256(image).hexdigest(),signing_key_id=key_id)
        source=root/'canonical.bin';sigfile=root/'signature.bin'
        def sign(manifest,*,salt=32,mgf='sha256',padding='pss',key_path=private,canonical=None):
            canonical=signing_bytes(manifest) if canonical is None else canonical
            source.write_bytes(canonical)
            options=['-sigopt',f'rsa_padding_mode:{padding}']
            if padding=='pss':options+=['-sigopt',f'rsa_pss_saltlen:{salt}','-sigopt',f'rsa_mgf1_md:{mgf}']
            run('dgst','-sha256','-sign',key_path,'-out',sigfile,*options,source)
            signature=sigfile.read_bytes()
            envelope=json.dumps(dict(manifest=manifest,signature_algorithm=ALGORITHM,signature=_b64(signature)),
                separators=(',',':'),ensure_ascii=True).encode()
            return envelope,canonical,signature
        original,canonical,signature=sign(base)
        bundle=dict(schema_version=1,hardware_id=hardware,inactive_capacity=1048576,accepted_counter=floor,
            key_id=key_id,public_key_der_base64=_b64(der),public_key_pem=public.read_text(),image_base64=_b64(image),cases=[])
        def add(name,*,packet=original,signbytes=canonical,signature_bytes=signature,
                manifest_ok=True,package_ok=None,crypto_ok=True,**overrides):
            case=dict(name=name,envelope_base64=_b64(packet),expected_manifest=manifest_ok,
                expected_package=manifest_ok if package_ok is None else package_ok,
                canonical_base64=_b64(signbytes),canonical_sha256=hashlib.sha256(signbytes).hexdigest(),
                signature_base64=_b64(signature_bytes),crypto_valid=crypto_ok,**overrides)
            bundle['cases'].append(case)
        add('valid')
        parsed=json.loads(original)
        add('valid_reordered_whitespace',packet=json.dumps(dict(reversed(list(parsed.items()))),indent=1).encode())
        add('valid_ascii_escape',packet=original.replace(b'TEST-ONLY-NOT-A0',b'\\u0054EST-ONLY-NOT-A0'))
        escaped_signature=''.join('\\u%04x'%byte for byte in _b64(signature).encode()).encode()
        escaped_packet=original.replace(_b64(signature).encode(),escaped_signature)
        assert len(escaped_packet)<=4096
        add('valid_escaped_signature',packet=escaped_packet)
        add('valid_escaped_manifest_key',packet=original.replace(b'"hardware_id"',b'"hardware_\\u0069d"'))
        longest=dict(base,hardware_id='H'*64,signing_key_id='K'*64,firmware_version='v'*31,
                     release_counter='18446744073709551615')
        packet,body,sig=sign(longest)
        add('valid_max_profile',packet=packet,signbytes=body,signature_bytes=sig,
            hardware_id=longest['hardware_id'],key_id=longest['signing_key_id'])
        for name,value in [('valid_release_max','18446744073709551615')]:
            changed=dict(base,release_counter=value);packet,body,sig=sign(changed)
            add(name,packet=packet,signbytes=body,signature_bytes=sig)
        add('hardware_mismatch',manifest_ok=False,hardware_id='OTHER-HARDWARE')
        add('slot_too_small',manifest_ok=False,inactive_capacity=len(image)-1)
        add('counter_replay',manifest_ok=False,accepted_counter=base['release_counter'])
        add('counter_higher_floor',manifest_ok=False,accepted_counter='18446744073709551615')
        add('unknown_key',manifest_ok=False,key_id='other-key')
        add('image_mutated',package_ok=False,image_base64=_b64(image[:-1]+bytes((image[-1]^1,))))
        add('image_trailing',package_ok=False,image_base64=_b64(image+b'\0'))
        add('image_truncated',package_ok=False,image_base64=_b64(image[:-1]))
        add('signature_bitflip',packet=original.replace(_b64(signature).encode(),_b64(bytes((signature[0]^1,))+signature[1:]).encode()),manifest_ok=False)
        for name,kwargs in [('signature_salt20',dict(salt=20)),('signature_mgf_sha384',dict(mgf='sha384')),
                            ('signature_pkcs1',dict(padding='pkcs1'))]:
            packet,body,sig=sign(base,**kwargs)
            add(name,packet=packet,signbytes=body,signature_bytes=sig,manifest_ok=False,crypto_ok=False)
        wrong_domain=canonical.replace(DOMAIN,b'AUTO-GAUGE-OTA-WRONG\0V1\0',1)
        packet,_,sig=sign(base,canonical=wrong_domain)
        add('signature_wrong_domain',packet=packet,signature_bytes=sig,manifest_ok=False,crypto_ok=False)
        packet,body,sig=sign(base,key_path=private_small)
        add('rsa3070_key',packet=packet,signbytes=body,signature_bytes=sig,manifest_ok=False,
            key_der_base64=_b64(der_small),key_pem=public_small.read_text(),crypto_ok=False)
        spki=run('pkey','-in',private,'-pubout','-outform','DER')
        add('key_spki_not_pkcs1',manifest_ok=False,key_der_base64=_b64(spki))
        add('key_der_trailing',manifest_ok=False,key_der_base64=_b64(der+b'\0'))
        add('key_der_garbage',manifest_ok=False,key_der_base64=_b64(b'\0'*32))
        replacements=[('schema_bool',b'"schema_version":1',b'"schema_version":true'),
            ('schema_float',b'"schema_version":1',b'"schema_version":1.0'),
            ('image_bytes_exponent',str(len(image)).encode(),str(len(image)).encode()+b'e0'),
            ('image_bytes_negative',b'"image_bytes":'+str(len(image)).encode(),b'"image_bytes":-1'),
            ('counter_leading_zero',b'"release_counter":"9007199254740993"',b'"release_counter":"09007199254740993"'),
            ('counter_overflow',b'"release_counter":"9007199254740993"',b'"release_counter":"18446744073709551616"'),
            ('hash_uppercase',base['image_sha256'].encode(),base['image_sha256'].upper().encode()),
            ('target_wrong',b'"target":"esp32s3"',b'"target":"esp32"'),
            ('signature_padding',_b64(signature).encode(),_b64(signature).encode()+b'='),
            ('signature_whitespace',_b64(signature).encode(),b' '+_b64(signature).encode())]
        for name,old,new in replacements:
            assert old in original
            add(name,packet=original.replace(old,new,1),manifest_ok=False)
        add('duplicate_manifest_key',packet=original.replace(b'"hardware_id":',b'"hardware_id":"TEST-ONLY-NOT-A0","hardware_id":',1),manifest_ok=False)
        add('duplicate_escaped_key',packet=original.replace(b'"hardware_id":',b'"\\u0068ardware_id":"TEST-ONLY-NOT-A0","hardware_id":',1),manifest_ok=False)
        add('duplicate_envelope_key',packet=original[:-1]+b',"signature_algorithm":"RSA-PSS-SHA256"}',manifest_ok=False)
        add('unknown_manifest_key',packet=original.replace(b'"schema_version":1',b'"schema_version":1,"extra":0',1),manifest_ok=False)
        add('unknown_envelope_key',packet=original[:-1]+b',"extra":0}',manifest_ok=False)
        add('trailing_json',packet=original+b'{}',manifest_ok=False)
        add('truncated_json',packet=original[:-1],manifest_ok=False)
        add('deep_json',packet=b'{"manifest":'+b'['*8+b'0'+b']'*8+b'}',manifest_ok=False)
        add('embedded_nul',packet=original.replace(b'TEST-ONLY-NOT-A0',b'TEST\\u0000-ONLY-NOT-A0'),manifest_ok=False)
        add('oversized_envelope',packet=original+b' '*4096,manifest_ok=False)
        for name,changed,variant_image in [('esp_version32',dict(base,firmware_version='v'*32),image),
                ('esp_image_unaligned',dict(base,image_bytes=len(image)+1,image_sha256=hashlib.sha256(image+b'\0').hexdigest()),image+b'\0')]:
            packet,body,sig=sign(changed)
            add(name,packet=packet,signbytes=body,signature_bytes=sig,manifest_ok=False,image_base64=_b64(variant_image))
        # Run the original verifier on every vector before exposing the bundle.
        # Reference PEM behavior is intentionally distinct from strict PKCS1 DER
        # or the ESP candidate's31-char/16-byte profile.
        image_path=root/'candidate.bin';key_path=root/'candidate-public.pem'
        for case in bundle['cases']:
            image_path.write_bytes(base64.b64decode(case.get('image_base64',bundle['image_base64'])))
            key_path.write_text(case.get('key_pem',bundle['public_key_pem']))
            try:
                verify_package(base64.b64decode(case['envelope_base64']),image_path,
                    trusted_keys={case.get('key_id',key_id):key_path},
                    hardware_id=case.get('hardware_id',hardware),
                    inactive_capacity=case.get('inactive_capacity',bundle['inactive_capacity']),
                    accepted_counter=int(case.get('accepted_counter',floor)),openssl=executable)
                case['expected_reference']=True
            except PackageError:case['expected_reference']=False
        # No private key/path is returned; all private files are deleted here.
        return bundle


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    args.output.write_text(json.dumps(generate_vectors(),indent=2)+'\n')
    print(args.output)


if __name__=='__main__':main()

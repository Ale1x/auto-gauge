#!/usr/bin/env python3
"""Optional local real-crypto check using a COPY of the already cached pinned SDK.

No download, SDK edits, private product keys, target execution or flash access.
Portable CI keeps its independent behavioral harness; this check requires cache.
"""
import argparse
import hashlib
import importlib.util
import json
import io
from pathlib import Path
import tarfile
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
SDK_COMMIT='fff9895c82d744c7237be8847347bdd1b07c6643'
PSA_COMMIT='a2b32072ea898afc1ed5b6caf6931e36028c91d6'


def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value


def checked_checkout(path, expected_revision, *, include_ignored=False):
    """Require a pinned clean checkout; no cache file is changed by this check."""
    revision = subprocess.check_output(
        ['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != expected_revision:
        raise RuntimeError('Pinned SDK/PSA checkout required; this command never downloads sources')
    command = ['git', '-C', str(path), 'status', '--porcelain=v1',
               '--untracked-files=all', '--ignore-submodules=none']
    if include_ignored:
        command.append('--ignored=matching')
    if subprocess.check_output(command):
        raise RuntimeError(f'Clean SDK/PSA checkout required: {path}')


def committed_snapshot(path, revision, destination):
    """Build from committed blobs, never from files that can change after status.

    The pinned PSA tree has no symlinks/gitlinks. Reject either explicitly rather
    than silently leaving a submodule absent or following an external target.
    Archive attributes may exclude/substitute files: the exact resulting archive
    digest is recorded alongside the commit, so the compiled snapshot is auditable.
    """
    entries = subprocess.check_output(
        ['git', '-C', str(path), 'ls-tree', '-rz', '-r', revision]).split(b'\0')
    if any(entry and entry.split(b' ', 1)[0] not in (b'100644', b'100755') for entry in entries):
        raise RuntimeError('PSA snapshot must contain only regular committed files')
    archive = subprocess.check_output(['git', '-C', str(path), 'archive', '--format=tar', revision])
    destination.mkdir()
    with tarfile.open(fileobj=io.BytesIO(archive), mode='r:') as bundle:
        for member in bundle.getmembers():
            name = Path(member.name)
            if name.is_absolute() or '..' in name.parts or not (member.isdir() or member.isfile()):
                raise RuntimeError('Unsafe entry in committed PSA snapshot')
        bundle.extractall(destination, filter='data')
    return dict(strategy='GIT_ARCHIVE_PINNED_COMMIT_NO_WORKTREE_COPY',
                archive_sha256=hashlib.sha256(archive).hexdigest(),
                tree=subprocess.check_output(
                    ['git', '-C', str(path), 'rev-parse', revision + '^{tree}'], text=True).strip(),
                sdk_status='CLEAN_INCLUDING_UNTRACKED_AND_SUBMODULES',
                psa_status='CLEAN_INCLUDING_UNTRACKED_IGNORED_AND_SUBMODULES')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk',type=Path,default=Path.home()/'.cache/auto-gauge/esp-idf-v6.1')
    parser.add_argument('--report',type=Path,default=ROOT/'firmware/ota_study/native-crypto-report.json')
    args=parser.parse_args();sdk=args.sdk.resolve()
    crypto=sdk/'components/mbedtls/mbedtls/tf-psa-crypto'
    checked_checkout(sdk, SDK_COMMIT)
    checked_checkout(crypto, PSA_COMMIT, include_ignored=True)
    vectors=module('ag_vectors_native',ROOT/'scripts/generate_ota_vectors.py').generate_vectors()
    header=module('ag_manifest_native_cases',ROOT/'tests/test_ota_manifest_target.py')
    with tempfile.TemporaryDirectory(prefix='ag-psa-native-') as directory:
        temp=Path(directory);source=temp/'source';source.mkdir()
        # GEN_FILES=OFF plus a committed snapshot prevents vendor source mutation.
        snapshot=committed_snapshot(crypto,PSA_COMMIT,source/'tf-psa-crypto')
        count=header.generate_header(vectors,source/'ota_manifest_vectors.h')
        shim=source/'shim';shim.mkdir()
        (shim/'esp_ota_ops.h').write_text('#include <stdint.h>\ntypedef struct ag_host_partition esp_partition_t;\ntypedef uint32_t esp_ota_handle_t;\n')
        compat=source/'host-compat/mbedtls';compat.mkdir(parents=True)
        # Pinned Espressif sources use legacy public forwarding headers from
        # its port. Native builds forward to the same portable private headers
        # without ESP accelerator declarations/config; no math is replaced.
        for name in ['bignum','cipher','ecdsa','ecp','rsa']:
            (compat/(name+'.h')).write_text('#define MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS\n#include "mbedtls/private/'+name+'.h"\n')
        production=ROOT/'firmware/ota_study/main'
        cmake=f'''cmake_minimum_required(VERSION 3.20)
project(AutoGaugeNativeCrypto LANGUAGES C)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(GEN_FILES OFF CACHE BOOL "" FORCE)
set(USE_SHARED_TF_PSA_CRYPTO_LIBRARY OFF CACHE BOOL "" FORCE)
include_directories(BEFORE "{source/'host-compat'}")
add_subdirectory(tf-psa-crypto)
add_executable(native_manifest "{production/'manifest_verify.c'}" "{ROOT/'firmware/ota_study/tests/manifest_native_driver.c'}")
target_include_directories(native_manifest PRIVATE "{production}" "{source}" "{shim}")
target_compile_options(native_manifest PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(native_manifest PRIVATE -fsanitize=address,undefined)
target_link_libraries(native_manifest PRIVATE tfpsacrypto)
'''
        (source/'CMakeLists.txt').write_text(cmake)
        build=temp/'build'
        subprocess.run(['cmake','-S',str(source),'-B',str(build),'-DCMAKE_BUILD_TYPE=Debug'],check=True)
        subprocess.run(['cmake','--build',str(build),'--target','native_manifest','--parallel','4'],check=True)
        result=subprocess.run([str(build/'native_manifest')],capture_output=True,text=True,check=True,timeout=60)
        print(result.stdout.strip())
        if f'{count} native PSA vectors' not in result.stdout:raise RuntimeError('Missing native verification result')
    files=['firmware/ota_study/main/manifest_verify.c','firmware/ota_study/main/manifest_verify.h',
           'firmware/ota_study/main/ota_installer.h',
           'firmware/ota_study/tests/manifest_native_driver.c','scripts/generate_ota_vectors.py',
           'scripts/check_native_ota_crypto.py','tests/test_ota_manifest_target.py']
    report=dict(status='NATIVE_PORTABLE_PSA_CRYPTO_PASS_NOT_TARGET_EXECUTION',sdk_commit=SDK_COMMIT,
        tf_psa_crypto_commit=PSA_COMMIT,vendor_snapshot=snapshot,vector_count=count,result=result.stdout.strip(),
        source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in files},
        actions=dict(native_crypto='EXECUTED',target_crypto='NOT_EXECUTED',flash='NOT_EXECUTED',
                     source_copy='TEMPORARY_REMOVED',product_keys='NOT_CREATED'),
        limitations=['Actual portable TF-PSA-Crypto RSA/SHA on native host, not ESP32 hardware acceleration/provider',
            'Production verifier/driver built with ASan/UBSan; vendor library built Debug without sanitizer instrumentation',
            'SDK handle/partition types are opaque; five native forwarding headers omit ESP accelerator declarations',
            'Portable default crypto configuration, no PSA/hash/RSA function or arithmetic is replaced',
            'Ephemeral OpenSSL public vectors; private test keys removed before native compilation',
            'No boot/flash/rollback/eFuse/security-key provisioning or physical entropy qualification'])
    args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(args.report)


if __name__=='__main__':main()

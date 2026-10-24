"""Read-only OTA package reference. Never flashes, changes boot slots or provisions keys."""
from __future__ import annotations

import argparse
import base64
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

MAX_ENVELOPE = 4096
DOMAIN = b'AUTO-GAUGE-OTA-MANIFEST\x00V1\x00'
ALGORITHM = 'RSA-PSS-SHA256'
FIELDS = {'schema_version', 'target', 'hardware_id', 'firmware_version',
          'release_counter', 'image_bytes', 'image_sha256', 'signing_key_id'}


class PackageError(ValueError):
    pass


def counter(value: object) -> int:
    if not isinstance(value, str) or not re.fullmatch(r'0|[1-9][0-9]{0,19}', value):
        raise PackageError('release_counter must be a canonical uint64 string')
    result = int(value)
    if result > 2**64 - 1:
        raise PackageError('release_counter overflow')
    return result


def _pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise PackageError('duplicate JSON key')
        result[key] = value
    return result


def _reject_number(_):
    raise PackageError('noninteger JSON number')


def _depth(data: bytes) -> None:
    depth = 0
    quoted = escaped = False
    for byte in data:
        if quoted:
            if escaped:
                escaped = False
            elif byte == 92:
                escaped = True
            elif byte == 34:
                quoted = False
        elif byte == 34:
            quoted = True
        elif byte in (91, 123):
            depth += 1
            if depth > 4:
                raise PackageError('JSON depth limit')
        elif byte in (93, 125):
            depth -= 1
            if depth < 0:
                raise PackageError('invalid JSON nesting')


def validate_manifest(value: object) -> dict:
    if type(value) is not dict or set(value) != FIELDS:
        raise PackageError('manifest keys differ from schema 1')
    if type(value['schema_version']) is not int or value['schema_version'] != 1:
        raise PackageError('unsupported schema')
    if value['target'] != 'esp32s3':
        raise PackageError('unsupported target')
    for name in ('hardware_id', 'signing_key_id'):
        if not isinstance(value[name], str) or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]{0,63}', value[name]):
            raise PackageError(f'invalid {name}')
    version = value['firmware_version']
    if not isinstance(version, str) or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9.+_-]{0,63}', version):
        raise PackageError('invalid firmware_version')
    counter(value['release_counter'])
    size = value['image_bytes']
    if type(size) is not int or not 1 <= size <= 16 * 1024 * 1024:
        raise PackageError('image size outside reference bound')
    digest = value['image_sha256']
    if not isinstance(digest, str) or not re.fullmatch(r'[0-9a-f]{64}', digest):
        raise PackageError('invalid image digest')
    return dict(value)


def signing_bytes(manifest: dict) -> bytes:
    body = validate_manifest(manifest)
    return DOMAIN + json.dumps(body, sort_keys=True, separators=(',', ':'), ensure_ascii=True).encode('ascii')


@dataclass(frozen=True)
class VerifiedPackage:
    hardware_id: str
    firmware_version: str
    release_counter: int
    image_bytes: int
    image_sha256: str
    signing_key_id: str


def verify_package(envelope: bytes, image: Path, *, trusted_keys: dict[str, Path],
                   hardware_id: str, inactive_capacity: int, accepted_counter: int,
                   openssl: str = 'openssl') -> VerifiedPackage:
    """Caller supplies trust anchors, actual slot capacity and durable replay floor.

    This validates a package snapshot, not a future read/write of the image file.
    No state is advanced: commit/boot/health/floor sequencing belongs to the target.
    """
    if type(envelope) is not bytes or not 1 <= len(envelope) <= MAX_ENVELOPE:
        raise PackageError('envelope size limit')
    if type(inactive_capacity) is not int or inactive_capacity <= 0:
        raise PackageError('invalid inactive capacity')
    if type(accepted_counter) is not int or not 0 <= accepted_counter <= 2**64 - 1:
        raise PackageError('invalid trusted replay floor')
    _depth(envelope)
    try:
        parsed = json.loads(envelope.decode('utf-8'), object_pairs_hook=_pairs,
                            parse_float=_reject_number, parse_constant=_reject_number)
    except (ValueError, UnicodeError, RecursionError) as exc:
        raise PackageError('invalid envelope JSON') from exc
    if type(parsed) is not dict or set(parsed) != {'manifest', 'signature_algorithm', 'signature'}:
        raise PackageError('invalid envelope keys')
    manifest = validate_manifest(parsed['manifest'])
    if parsed['signature_algorithm'] != ALGORITHM:
        raise PackageError('unsupported signature algorithm')
    if manifest['hardware_id'] != hardware_id:
        raise PackageError('hardware mismatch')
    release = counter(manifest['release_counter'])
    if release <= accepted_counter:
        raise PackageError('stale or replayed release')
    if manifest['image_bytes'] > inactive_capacity:
        raise PackageError('inactive slot too small')
    key = trusted_keys.get(manifest['signing_key_id'])
    if key is None:
        raise PackageError('untrusted signing key')
    try:
        with key.open('rb') as key_stream:
            key_bytes = key_stream.read(8193)
        if not 1 <= len(key_bytes) <= 8192:
            raise PackageError('public key size limit')
        signature_text = parsed['signature']
        if not isinstance(signature_text, str):
            raise PackageError('invalid signature type')
        signature = base64.b64decode(signature_text, validate=True)
        if len(signature) != 384 or base64.b64encode(signature).decode('ascii') != signature_text:
            raise PackageError('expected canonical RSA-3072 signature')
        executable = shutil.which(openssl)
        if executable is None:
            raise PackageError('OpenSSL unavailable; verification cannot be skipped')
        with tempfile.TemporaryDirectory(prefix='ag-ota-verify-') as temporary:
            root = Path(temporary)
            (root/'public.pem').write_bytes(key_bytes)
            description = subprocess.run([executable, 'pkey', '-pubin', '-in', str(root/'public.pem'),
                '-text', '-noout'], capture_output=True, timeout=10, check=False)
            if description.returncode != 0 or not re.search(
                    rb'^Public-Key: \(3072 bit\)\r?\nModulus:\r?$', description.stdout, re.MULTILINE):
                raise PackageError('trusted key must be exactly RSA-3072')
            (root/'manifest.bin').write_bytes(signing_bytes(manifest))
            (root/'signature.bin').write_bytes(signature)
            result = subprocess.run([executable, 'dgst', '-sha256', '-verify', str(root/'public.pem'),
                '-signature', str(root/'signature.bin'), '-sigopt', 'rsa_padding_mode:pss',
                '-sigopt', 'rsa_pss_saltlen:32', '-sigopt', 'rsa_mgf1_md:sha256',
                str(root/'manifest.bin')], capture_output=True, timeout=10, check=False)
            if result.returncode != 0:
                raise PackageError('signature verification failed')
        digest = hashlib.sha256()
        total = 0
        with image.open('rb') as stream:
            while chunk := stream.read(65536):
                total += len(chunk)
                if total > manifest['image_bytes']:
                    raise PackageError('image contains trailing or oversized data')
                digest.update(chunk)
        if total != manifest['image_bytes'] or digest.hexdigest() != manifest['image_sha256']:
            raise PackageError('image size or digest mismatch')
    except (OSError, ValueError, subprocess.TimeoutExpired) as exc:
        if isinstance(exc, PackageError):
            raise
        raise PackageError('package verification failed') from exc
    return VerifiedPackage(hardware_id, manifest['firmware_version'], release,
                           total, digest.hexdigest(), manifest['signing_key_id'])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('envelope', type=Path)
    parser.add_argument('image', type=Path)
    parser.add_argument('--public-key', type=Path, required=True)
    parser.add_argument('--key-id', required=True)
    parser.add_argument('--hardware-id', required=True)
    parser.add_argument('--inactive-capacity', type=int, required=True)
    parser.add_argument('--accepted-counter', type=int, required=True)
    args = parser.parse_args()
    try:
        with args.envelope.open('rb') as stream:
            envelope = stream.read(MAX_ENVELOPE + 1)
        verified = verify_package(envelope, args.image,
            trusted_keys={args.key_id: args.public_key}, hardware_id=args.hardware_id,
            inactive_capacity=args.inactive_capacity, accepted_counter=args.accepted_counter)
    except (OSError, PackageError) as exc:
        raise SystemExit(f'REJECTED: {exc}') from exc
    print(f'VERIFIED READ-ONLY: release={verified.release_counter}, bytes={verified.image_bytes}')
    print('No image-format/bootloader validation, flash, boot selection or durable counter update performed.')


if __name__ == '__main__':
    main()

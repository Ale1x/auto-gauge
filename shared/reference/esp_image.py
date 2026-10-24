"""Bounded, read-only ESP32-S3 application preflight; never a bootloader validator.

Strict development profile: unsigned image container, appended SHA256 required.
Secure Boot signature sectors are deliberately rejected until independently verified.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass, field
import hashlib
from pathlib import Path
import struct
import tempfile

from ota_manifest import MAX_ENVELOPE, PackageError, VerifiedPackage, verify_package

MAX_IMAGE = 16 * 1024 * 1024


class ImageError(PackageError):
    pass


@dataclass(frozen=True)
class ImageInfo:
    segment_count: int
    entry_address: int
    minimum_revision: int
    maximum_revision: int
    secure_version: int
    firmware_version: str
    project_name: str
    image_sha256: str


def _text(data: bytes, name: str) -> str:
    head, separator, tail = data.partition(b'\0')
    if not separator or not head or any(tail) or any(x < 32 or x > 126 for x in head):
        raise ImageError(f'invalid application {name}')
    return head.decode('ascii')


def inspect_image(image: bytes, *, chip_revision: int, inactive_capacity: int,
                  minimum_secure_version: int) -> ImageInfo:
    """Check a fixed immutable snapshot; trusted constraints never come from image.

    Does not validate memory mappings, eFuse policy, image signatures or bootability.
    Revision is major*100+minor, explicitly supplied by the caller.
    """
    for value, limit, name in ((chip_revision, 65535, 'revision'),
                               (minimum_secure_version, 2**32-1, 'secure version'),
                               (inactive_capacity, MAX_IMAGE, 'capacity')):
        if type(value) is not int or not 0 <= value <= limit:
            raise ImageError(f'invalid trusted {name}')
    if type(image) is not bytes or not 24 <= len(image) <= min(inactive_capacity, MAX_IMAGE):
        raise ImageError('image size outside capacity/bound')
    if image[0] != 0xE9 or not 1 <= image[1] <= 16:
        raise ImageError('invalid image magic or segment count')
    if struct.unpack_from('<H', image, 12)[0] != 9:
        raise ImageError('image is not ESP32-S3')
    if image[19:23] != bytes(4) or image[23] != 1:
        raise ImageError('unsupported header or missing appended hash')
    minimum, maximum = struct.unpack_from('<HH', image, 15)
    # 0 and 0xffff are unset max fields in this SDK. No eFuse bypass is assumed.
    if maximum not in (0, 65535) and maximum < minimum:
        raise ImageError('inverted chip revision range')
    if chip_revision < minimum or (maximum not in (0, 65535) and chip_revision > maximum):
        raise ImageError('chip revision outside image range')
    offset, checksum = 24, 0xEF
    descriptor = None
    for index in range(image[1]):
        if offset + 8 > len(image):
            raise ImageError('truncated segment header')
        address, length = struct.unpack_from('<II', image, offset)
        offset += 8
        if length % 4 or length >= MAX_IMAGE or address + length > 2**32 or offset + length > len(image):
            raise ImageError('invalid segment extent')
        segment = image[offset:offset + length]
        if index == 0:
            if len(segment) < 256 or struct.unpack_from('<I', segment)[0] != 0xABCD5432:
                raise ImageError('missing application descriptor (bootloader rejected)')
            descriptor = segment[:256]
        for value in segment:
            checksum ^= value
        offset += length
    checksum_end = (offset + 1 + 15) & ~15
    if checksum_end + 32 != len(image):
        raise ImageError('truncated image or unsupported trailing/signature data')
    if any(image[offset:checksum_end-1]) or image[checksum_end-1] != checksum:
        raise ImageError('invalid checksum/padding')
    if hashlib.sha256(image[:checksum_end]).digest() != image[checksum_end:]:
        raise ImageError('invalid appended SHA256')
    assert descriptor is not None
    secure_version = struct.unpack_from('<I', descriptor, 4)[0]
    if secure_version < minimum_secure_version:
        raise ImageError('application secure version below trusted floor')
    return ImageInfo(image[1], struct.unpack_from('<I', image, 4)[0], minimum, maximum,
                     secure_version, _text(descriptor[16:48], 'version'),
                     _text(descriptor[48:80], 'project name'), hashlib.sha256(image).hexdigest())


@dataclass(frozen=True)
class VerifiedCandidate:
    package: VerifiedPackage
    info: ImageInfo
    image: bytes = field(repr=False)


def verify_candidate(envelope: bytes, image_path: Path, *, chip_revision: int,
                     minimum_secure_version: int, **package_options) -> VerifiedCandidate:
    """Verify manifest and format on the SAME bounded snapshot; retain those bytes.

    Never reopen the caller's path after verification. A later target installer must
    verify the actual flashed bytes and SDK signature/partition/eFuse constraints.
    """
    try:
        with image_path.open('rb') as source:
            snapshot = source.read(MAX_IMAGE + 1)
    except OSError as exc:
        raise ImageError('cannot read candidate') from exc
    if len(snapshot) > MAX_IMAGE:
        raise ImageError('candidate exceeds reference bound')
    # Reuse the signature verifier without exposing a writable caller-controlled path.
    with tempfile.TemporaryDirectory(prefix='ag-esp-candidate-') as directory:
        path = Path(directory)/'snapshot.bin'
        path.write_bytes(snapshot)
        package = verify_package(envelope, path, **package_options)
    info = inspect_image(snapshot, chip_revision=chip_revision,
                         inactive_capacity=package_options['inactive_capacity'],
                         minimum_secure_version=minimum_secure_version)
    if info.firmware_version != package.firmware_version:
        raise ImageError('manifest/application firmware version mismatch')
    return VerifiedCandidate(package, info, snapshot)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--chip-revision', type=int, required=True)
    parser.add_argument('--inactive-capacity', type=int, required=True)
    parser.add_argument('--minimum-secure-version', type=int, required=True)
    parser.add_argument('--envelope', type=Path)
    parser.add_argument('--public-key', type=Path)
    parser.add_argument('--key-id')
    parser.add_argument('--hardware-id')
    parser.add_argument('--accepted-counter', type=int)
    args = parser.parse_args()
    authentication = (args.envelope, args.public_key, args.key_id, args.hardware_id, args.accepted_counter)
    signed = any(value is not None for value in authentication)
    if signed and any(value is None for value in authentication):
        parser.error('signed preflight requires envelope, public-key, key-id, hardware-id and accepted-counter')
    try:
        if signed:
            with args.envelope.open('rb') as source:
                envelope = source.read(MAX_ENVELOPE + 1)
            candidate = verify_candidate(envelope, args.image, chip_revision=args.chip_revision,
                minimum_secure_version=args.minimum_secure_version,
                trusted_keys={args.key_id: args.public_key}, hardware_id=args.hardware_id,
                inactive_capacity=args.inactive_capacity, accepted_counter=args.accepted_counter)
            result, snapshot = candidate.info, candidate.image
        else:
            with args.image.open('rb') as source:
                snapshot = source.read(MAX_IMAGE + 1)
            result = inspect_image(snapshot, chip_revision=args.chip_revision,
                inactive_capacity=args.inactive_capacity, minimum_secure_version=args.minimum_secure_version)
    except (OSError, PackageError) as exc:
        raise SystemExit(f'REJECTED: {exc}') from exc
    label = 'SIGNED MANIFEST + FORMAT PREFLIGHT' if signed else 'FORMAT PREFLIGHT ONLY'
    print(f'{label}: {result.project_name} {result.firmware_version}, {len(snapshot)} bytes')
    print('No Secure Boot signature/bootability qualification, flash, boot selection or counter update.')


if __name__ == '__main__':
    main()

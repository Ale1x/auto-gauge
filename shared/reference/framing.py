"""Bounded development framing; CRC is integrity checking, not authentication."""
from dataclasses import dataclass
import struct
import zlib

MAX_PAYLOAD = 4096
MAX_FRAME = MAX_PAYLOAD + 16
MAX_CHUNK = 512
ASSEMBLY_TIMEOUT_MS = 1000  # Development assumption, not a product timeout.
HEADER = struct.Struct('<2sBBIHH')


class FrameError(ValueError):
    pass


@dataclass(frozen=True)
class Frame:
    kind: int
    request_id: int
    payload: bytes


def check_header(header):
    magic, version, kind, request_id, length, reserved = HEADER.unpack(header)
    if magic != b'AG' or version != 1 or reserved != 0:
        raise FrameError('Invalid header')
    if kind not in (1, 2, 3) or (request_id == 0) != (kind == 3):
        raise FrameError('Invalid kind/request ID')
    if length > MAX_PAYLOAD:
        raise FrameError('Payload too large')
    return kind, request_id, length


def encode(frame):
    if type(frame.kind) is not int or type(frame.request_id) is not int or not 0 <= frame.request_id <= 0xffffffff:
        raise FrameError('Invalid header field')
    if not isinstance(frame.payload, bytes) or len(frame.payload) > MAX_PAYLOAD:
        raise FrameError('Invalid payload')
    if frame.kind not in (1, 2, 3) or (frame.request_id == 0) != (frame.kind == 3):
        raise FrameError('Invalid kind/request ID')
    header = HEADER.pack(b'AG', 1, frame.kind, frame.request_id, len(frame.payload), 0)
    packet = header + frame.payload
    return packet + struct.pack('<I', zlib.crc32(packet))


def decode(packet):
    if len(packet) < 16 or len(packet) > MAX_FRAME:
        raise FrameError('Invalid packet length')
    kind, request_id, length = check_header(packet[:12])
    if len(packet) != 16 + length:
        raise FrameError('Length mismatch')
    if zlib.crc32(packet[:-4]) != struct.unpack('<I', packet[-4:])[0]:
        raise FrameError('CRC mismatch')
    return Frame(kind, request_id, bytes(packet[12:-4]))


class StreamDecoder:
    def __init__(self):
        self.buffer = bytearray()
        self.started_ms = None
        self.last_ms = None

    def reset(self):
        self.buffer.clear()
        self.started_ms = None

    def feed(self, chunk, now_ms):
        if type(now_ms) is not int or now_ms < 0 or (self.last_ms is not None and now_ms < self.last_ms):
            self.reset()
            raise FrameError('Nonmonotonic clock')
        self.last_ms = now_ms
        try:
            if len(chunk) > MAX_CHUNK:
                raise FrameError('Chunk exceeds development transport limit')
            if self.started_ms is not None and now_ms - self.started_ms >= ASSEMBLY_TIMEOUT_MS:
                raise FrameError('Incomplete frame expired')
            frames = []
            # Process at most one incomplete frame; never append an entire
            # untrusted stream before inspecting its advertised length.
            for octet in chunk:
                if self.started_ms is None:
                    self.started_ms = now_ms
                self.buffer.append(octet)
                if len(self.buffer) < 12:
                    continue
                _, _, size = check_header(self.buffer[:12])
                if len(self.buffer) == size + 16:
                    frames.append(decode(self.buffer))
                    self.reset()
            return frames
        except (ValueError, TypeError):
            self.reset()
            raise

import Foundation

public enum GaugeFrameKind: UInt8, Sendable {
    case request = 1, response = 2, event = 3
}

public struct GaugeFrame: Equatable, Sendable {
    public var kind: GaugeFrameKind
    public var requestID: UInt32
    public var payload: Data
    public init(kind: GaugeFrameKind, requestID: UInt32, payload: Data) {
        self.kind = kind; self.requestID = requestID; self.payload = payload
    }
}

public enum FrameCodecError: Error, LocalizedError {
    case length, magic, version, kind, requestID, reserved, checksum
    public var errorDescription: String? {
        switch self {
        case .length: return "Lunghezza del messaggio non valida."
        case .magic: return "Intestazione del messaggio non valida."
        case .version: return "Versione del messaggio non supportata."
        case .kind: return "Tipo di messaggio non valido."
        case .requestID: return "Identificatore della richiesta non valido."
        case .reserved: return "Campi riservati del messaggio non validi."
        case .checksum: return "Controllo di integrità del messaggio non riuscito."
        }
    }
}

/// Development packet codec only. CRC does not authenticate, encrypt or prevent replay.
/// No CoreBluetooth, reassembly, timers or device mutation is implemented here.
public enum GaugeFrameCodec {
    public static let maximumPayload = 4096
    public static let overhead = 16

    public static func encode(_ frame: GaugeFrame) throws -> Data {
        guard frame.payload.count <= maximumPayload else { throw FrameCodecError.length }
        try checkRequestID(frame.kind, frame.requestID)
        let length = frame.payload.count
        var bytes: [UInt8] = [0x41, 0x47, 1, frame.kind.rawValue]
        appendLE(frame.requestID, to: &bytes)
        bytes += [UInt8(length & 0xff), UInt8((length >> 8) & 0xff), 0, 0]
        bytes += frame.payload
        appendLE(crc32(bytes), to: &bytes)
        return Data(bytes)
    }

    public static func decode(_ packet: Data) throws -> GaugeFrame {
        guard (overhead...(maximumPayload + overhead)).contains(packet.count) else { throw FrameCodecError.length }
        // Data slices can have nonzero startIndex: normalize only after the bounded size check.
        let bytes = Array(packet)
        guard bytes[0] == 0x41, bytes[1] == 0x47 else { throw FrameCodecError.magic }
        guard bytes[2] == 1 else { throw FrameCodecError.version }
        guard let kind = GaugeFrameKind(rawValue: bytes[3]) else { throw FrameCodecError.kind }
        let requestID = readLE(bytes, at: 4)
        try checkRequestID(kind, requestID)
        let length = Int(bytes[8]) | Int(bytes[9]) << 8
        guard length <= maximumPayload, bytes.count == length + overhead else { throw FrameCodecError.length }
        guard bytes[10] == 0, bytes[11] == 0 else { throw FrameCodecError.reserved }
        let checksumIndex = 12 + length
        guard crc32(bytes[..<checksumIndex]) == readLE(bytes, at: checksumIndex) else { throw FrameCodecError.checksum }
        return GaugeFrame(kind: kind, requestID: requestID, payload: Data(bytes[12..<checksumIndex]))
    }

    public static func crc32<S: Sequence>(_ bytes: S) -> UInt32 where S.Element == UInt8 {
        var crc: UInt32 = 0xffffffff
        for byte in bytes {
            crc ^= UInt32(byte)
            for _ in 0..<8 { crc = crc & 1 == 1 ? (crc >> 1) ^ 0xedb88320 : crc >> 1 }
        }
        return crc ^ 0xffffffff
    }
    private static func checkRequestID(_ kind: GaugeFrameKind, _ id: UInt32) throws {
        guard kind == .event ? id == 0 : id != 0 else { throw FrameCodecError.requestID }
    }
    private static func readLE(_ bytes: [UInt8], at offset: Int) -> UInt32 {
        UInt32(bytes[offset]) | UInt32(bytes[offset+1]) << 8 | UInt32(bytes[offset+2]) << 16 | UInt32(bytes[offset+3]) << 24
    }
    private static func appendLE(_ value: UInt32, to bytes: inout [UInt8]) {
        for shift in stride(from: 0, to: 32, by: 8) { bytes.append(UInt8((value >> shift) & 0xff)) }
    }
}

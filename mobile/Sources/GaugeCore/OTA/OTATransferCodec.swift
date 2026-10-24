import Foundation

/// Payload codec only. Wire fields, offsets and IDs never authenticate a caller.
public enum OTATransferCodec {
    public static let maximumChunkBytes = 1024
    public static let maximumManifestBytes: UInt32 = 4096
    public static let maximumImageBytes: UInt32 = 16 * 1024 * 1024

    public static func encodeRequest(_ request: OTATransferRequest) throws -> Data {
        try validate(request)
        var bytes: [UInt8] = [1, request.opcode.rawValue, 0, 0]
        append(request.transferID, to: &bytes); append(request.offset, to: &bytes)
        bytes.append(contentsOf: request.body)
        return Data(bytes)
    }
    public static func decodeRequest(_ payload: Data) throws -> OTATransferRequest {
        guard (12...(12 + maximumChunkBytes)).contains(payload.count) else { throw OTATransferError.invalidPayload }
        let bytes = [UInt8](payload)
        guard bytes[0] == 1, bytes[2] == 0, bytes[3] == 0,
              let opcode = OTATransferOpcode(rawValue: bytes[1]) else { throw OTATransferError.invalidPayload }
        let request = OTATransferRequest(opcode: opcode, transferID: read(bytes, at: 4),
                                         offset: read(bytes, at: 8), body: Data(bytes.dropFirst(12)))
        try validate(request)
        return request
    }
    public static func encodeResponse(_ response: OTATransferResponse) throws -> Data {
        guard response.manifestOffset <= maximumManifestBytes, response.imageOffset <= maximumImageBytes else {
            throw OTATransferError.invalidPayload
        }
        var bytes: [UInt8] = [1, response.status.rawValue, response.phase.rawValue, response.selectionMayHaveChanged ? 1 : 0]
        append(response.transferID, to: &bytes); append(response.manifestOffset, to: &bytes); append(response.imageOffset, to: &bytes)
        return Data(bytes)
    }
    public static func decodeResponse(_ payload: Data) throws -> OTATransferResponse {
        guard payload.count == 16 else { throw OTATransferError.invalidPayload }
        let bytes = [UInt8](payload)
        guard bytes[0] == 1, bytes[3] <= 1, let status = OTATransferStatus(rawValue: bytes[1]),
              let phase = OTATransferPhase(rawValue: bytes[2]), read(bytes, at: 8) <= maximumManifestBytes,
              read(bytes, at: 12) <= maximumImageBytes else { throw OTATransferError.invalidPayload }
        return .init(status: status, phase: phase, selectionMayHaveChanged: bytes[3] == 1,
                     transferID: read(bytes, at: 4), manifestOffset: read(bytes, at: 8), imageOffset: read(bytes, at: 12))
    }
    private static func validate(_ request: OTATransferRequest) throws {
        guard request.transferID != 0 else { throw OTATransferError.invalidPayload }
        switch request.opcode {
        case .start:
            guard request.body.isEmpty, (1...maximumManifestBytes).contains(request.offset) else { throw OTATransferError.invalidPayload }
        case .manifest, .image:
            let maximum = request.opcode == .manifest ? maximumManifestBytes : maximumImageBytes
            guard (1...maximumChunkBytes).contains(request.body.count), request.offset <= maximum,
                  UInt32(request.body.count) <= maximum - request.offset else { throw OTATransferError.invalidPayload }
        case .begin, .finish, .select, .cancel:
            guard request.body.isEmpty, request.offset == 0 else { throw OTATransferError.invalidPayload }
        }
    }
    private static func append(_ value: UInt32, to bytes: inout [UInt8]) {
        for shift in stride(from: 0, to: 32, by: 8) { bytes.append(UInt8(truncatingIfNeeded: value >> shift)) }
    }
    private static func read(_ bytes: [UInt8], at offset: Int) -> UInt32 {
        (0..<4).reduce(0) { $0 | (UInt32(bytes[offset + $1]) << ($1 * 8)) }
    }
}

import Foundation
import XCTest
@testable import GaugeCore

final class OTATransferCodecTests: XCTestCase {
    private struct Fixture: Decodable {
        let schema_version: Int
        let requests: [RequestCase]
        let responses: [ResponseCase]
    }
    private struct RequestCase: Decodable {
        let name: String; let hex: String; let valid: Bool
        let opcode: UInt8?; let transfer_id: UInt32?; let offset: UInt32?; let body_hex: String?
    }
    private struct ResponseCase: Decodable {
        let name: String; let hex: String; let valid: Bool
        let status: UInt8?; let phase: UInt8?; let flags: UInt8?; let transfer_id: UInt32?
        let manifest_offset: UInt32?; let image_offset: UInt32?
    }
    private func bytes(_ hex: String) throws -> Data {
        guard hex.count.isMultiple(of: 2) else { throw OTATransferError.invalidPayload }
        var result = Data(); var at = hex.startIndex
        while at < hex.endIndex {
            let end = hex.index(at, offsetBy: 2)
            result.append(try XCTUnwrap(UInt8(hex[at..<end], radix: 16))); at = end
        }
        return result
    }
    func testSharedPayloadCasesAndExactRoundtrips() throws {
        let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
        let fixture = try JSONDecoder().decode(Fixture.self, from: Data(contentsOf: root.appendingPathComponent("shared/fixtures/ota-transfer.cases.json")))
        XCTAssertEqual(fixture.schema_version, 1)
        for vector in fixture.requests {
            let payload = try bytes(vector.hex)
            if vector.valid {
                let decoded = try OTATransferCodec.decodeRequest(payload)
                XCTAssertEqual(decoded.opcode.rawValue, vector.opcode, vector.name)
                XCTAssertEqual(decoded.transferID, vector.transfer_id, vector.name)
                XCTAssertEqual(decoded.offset, vector.offset, vector.name)
                XCTAssertEqual(decoded.body, try bytes(XCTUnwrap(vector.body_hex)), vector.name)
                XCTAssertEqual(try OTATransferCodec.encodeRequest(decoded), payload, vector.name)
            } else { XCTAssertThrowsError(try OTATransferCodec.decodeRequest(payload), vector.name) }
        }
        for vector in fixture.responses {
            let payload = try bytes(vector.hex)
            if vector.valid {
                let decoded = try OTATransferCodec.decodeResponse(payload)
                XCTAssertEqual(decoded.status.rawValue, vector.status, vector.name)
                XCTAssertEqual(decoded.phase.rawValue, vector.phase, vector.name)
                XCTAssertEqual(decoded.selectionMayHaveChanged, vector.flags == 1, vector.name)
                XCTAssertEqual(decoded.transferID, vector.transfer_id, vector.name)
                XCTAssertEqual(decoded.manifestOffset, vector.manifest_offset, vector.name)
                XCTAssertEqual(decoded.imageOffset, vector.image_offset, vector.name)
                XCTAssertEqual(try OTATransferCodec.encodeResponse(decoded), payload, vector.name)
            } else { XCTAssertThrowsError(try OTATransferCodec.decodeResponse(payload), vector.name) }
        }
    }
    func testInvalidModelsCannotBeEncodedAndSlicedDataHasNoIndexAssumption() throws {
        XCTAssertThrowsError(try OTATransferCodec.encodeRequest(.init(opcode: .start, transferID: 0, offset: 1)))
        XCTAssertThrowsError(try OTATransferCodec.encodeRequest(.init(opcode: .manifest, transferID: 1, offset: UInt32.max, body: Data([1]))))
        XCTAssertThrowsError(try OTATransferCodec.encodeRequest(.init(opcode: .image, transferID: 1, body: Data(repeating: 0, count: 1025))))
        XCTAssertThrowsError(try OTATransferCodec.encodeResponse(.init(status: .ok, phase: .writing, transferID: 1, manifestOffset: 4097, imageOffset: 0)))
        let packet = try OTATransferCodec.encodeRequest(.init(opcode: .image, transferID: UInt32.max, offset: 16_777_215, body: Data([255])))
        var padded = Data([0, 0]); padded.append(packet)
        XCTAssertEqual(try OTATransferCodec.decodeRequest(padded.dropFirst(2)).body, Data([255]))
        for size in 0..<12 { XCTAssertThrowsError(try OTATransferCodec.decodeRequest(Data(packet.prefix(size)))) }
    }
}

import Foundation
import XCTest
@testable import GaugeCore

final class FramingTests: XCTestCase {
    private struct Vector: Decodable {
        let name: String
        let valid: Bool
        let frame_hex: String
        let kind: UInt8?
        let request_id: UInt32?
        let payload_hex: String?
    }
    private func unhex(_ text: String) throws -> Data {
        guard text.count % 2 == 0 else { throw FrameCodecError.length }
        var bytes = [UInt8]()
        var index = text.startIndex
        while index < text.endIndex {
            let end = text.index(index, offsetBy: 2)
            guard let value = UInt8(text[index..<end], radix: 16) else { throw FrameCodecError.magic }
            bytes.append(value); index = end
        }
        return Data(bytes)
    }
    func testSharedGoldenPacketsAndMalformedPackets() throws {
        let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
        let data = try Data(contentsOf: root.appendingPathComponent("shared/fixtures/frame.cases.json"))
        let vectors = try JSONDecoder().decode([Vector].self, from: data)
        for vector in vectors {
            let packet = try unhex(vector.frame_hex)
            if vector.valid {
                let frame = try GaugeFrameCodec.decode(packet)
                XCTAssertEqual(frame.kind.rawValue, vector.kind, vector.name)
                XCTAssertEqual(frame.requestID, vector.request_id, vector.name)
                XCTAssertEqual(frame.payload, try unhex(XCTUnwrap(vector.payload_hex)), vector.name)
                XCTAssertEqual(try GaugeFrameCodec.encode(frame), packet, vector.name)
            } else { XCTAssertThrowsError(try GaugeFrameCodec.decode(packet), vector.name) }
        }
    }
    func testPayloadBoundsAndRequestIDRules() throws {
        let frame = GaugeFrame(kind: .response, requestID: .max, payload: Data(repeating: 0xff, count: 4096))
        let packet = try GaugeFrameCodec.encode(frame)
        XCTAssertEqual(packet.count, 4112)
        XCTAssertEqual(try GaugeFrameCodec.decode(packet), frame)
        XCTAssertThrowsError(try GaugeFrameCodec.encode(.init(kind: .request, requestID: 1, payload: Data(repeating: 0, count: 4097))))
        XCTAssertThrowsError(try GaugeFrameCodec.encode(.init(kind: .request, requestID: 0, payload: Data())))
        XCTAssertThrowsError(try GaugeFrameCodec.encode(.init(kind: .response, requestID: 0, payload: Data())))
        XCTAssertThrowsError(try GaugeFrameCodec.encode(.init(kind: .event, requestID: 1, payload: Data())))
    }
    func testEveryTruncationAndPayloadCorruptionIsRejected() throws {
        let packet = try GaugeFrameCodec.encode(.init(kind: .request, requestID: 42, payload: Data("Prova luminosità".utf8)))
        for length in 0..<packet.count { XCTAssertThrowsError(try GaugeFrameCodec.decode(Data(packet.prefix(length)))) }
        for index in 12..<(packet.count-4) {
            var corrupted = packet
            corrupted[index] ^= 1
            XCTAssertThrowsError(try GaugeFrameCodec.decode(corrupted))
        }
    }
    func testCRCReferenceAndSlicedData() throws {
        XCTAssertEqual(GaugeFrameCodec.crc32("123456789".utf8), 0xcbf43926)
        let frame = GaugeFrame(kind: .event, requestID: 0, payload: Data())
        let packet = try GaugeFrameCodec.encode(frame)
        let prefixed = Data([0xff, 0xff]) + packet
        XCTAssertEqual(try GaugeFrameCodec.decode(prefixed.dropFirst(2)), frame)
    }
}

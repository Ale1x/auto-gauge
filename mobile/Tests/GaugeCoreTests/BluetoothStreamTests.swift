import Foundation
import XCTest
@testable import GaugeCore

final class BluetoothStreamTests: XCTestCase {
    func testSharedGATTProfileResourceAndStrictNumbers() throws {
        let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
        let data = try Data(contentsOf: root.appendingPathComponent("shared/gatt.profile.json"))
        let expected = try GaugeGATTProfile.decode(data)
        let resource = try GaugeGATTProfile.development()
        XCTAssertEqual(expected.serviceUUID, resource.serviceUUID); XCTAssertEqual(expected.requestUUID, resource.requestUUID); XCTAssertEqual(expected.responseUUID, resource.responseUUID)
        var raw = try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [String: Any])
        raw["profile_version"] = true
        XCTAssertThrowsError(try GaugeGATTProfile.decode(JSONSerialization.data(withJSONObject: raw)))
        raw["profile_version"] = 1; raw["maximum_frame_bytes"] = 9999
        XCTAssertThrowsError(try GaugeGATTProfile.decode(JSONSerialization.data(withJSONObject: raw)))
    }
    func testMaximumFrameAcrossDifferentFragmentSizesAndNonzeroDataIndices() throws {
        let frame = GaugeFrame(kind: .response, requestID: 37, payload: Data(repeating: 0x41, count: 4096))
        let packet = try GaugeFrameCodec.encode(frame)
        for chunkSize in [1, 7, 12, 20, 185, 512] {
            var decoder = GaugeStreamReassembler(); var output: [GaugeFrame] = []
            let now = ContinuousClock().now
            var offset = 0
            while offset < packet.count {
                let end = min(packet.count, offset + chunkSize)
                output += try decoder.feed(packet[offset..<end], at: now)
                XCTAssertLessThanOrEqual(decoder.bufferedBytes, 4112)
                offset = end
            }
            XCTAssertEqual(output, [frame]); XCTAssertEqual(decoder.bufferedBytes, 0)
        }
    }
    func testCoalescedFramesAndPartialNextHeaderArePreserved() throws {
        let a = try GaugeFrameCodec.encode(.init(kind: .response, requestID: 1, payload: Data("A".utf8)))
        let b = try GaugeFrameCodec.encode(.init(kind: .response, requestID: 1, payload: Data("B".utf8)))
        var decoder = GaugeStreamReassembler(); let now = ContinuousClock().now
        let first = try decoder.feed(a + b.prefix(4), at: now)
        XCTAssertEqual(first.count, 1); XCTAssertEqual(decoder.bufferedBytes, 4)
        let second = try decoder.feed(Data(b.dropFirst(4)), at: now)
        XCTAssertEqual(second.count, 1); XCTAssertEqual(second.first?.payload, Data("B".utf8))
    }
    func testAbsoluteFrameTimeoutCannotBeExtendedByDripAndFiresWithoutNewBytes() throws {
        var decoder = GaugeStreamReassembler(); let now = ContinuousClock().now
        _ = try decoder.feed(Data([0x41]), at: now)
        _ = try decoder.feed(Data([0x47]), at: now.advanced(by: .seconds(9)))
        XCTAssertEqual(decoder.deadline, now.advanced(by: .seconds(10)))
        XCTAssertThrowsError(try decoder.checkTimeout(at: now.advanced(by: .seconds(10))))
        XCTAssertEqual(decoder.bufferedBytes, 0); XCTAssertNil(decoder.deadline)
    }
    func testMalformedHeadersAndCRCAbortWithoutMagicSearch() throws {
        let good = try GaugeFrameCodec.encode(.init(kind: .response, requestID: 1, payload: Data("AG AG".utf8)))
        for index in [0, 2, 3, 9, 10, good.count - 1] {
            var corrupt = good; corrupt[index] = 0xff
            var decoder = GaugeStreamReassembler()
            XCTAssertThrowsError(try decoder.feed(corrupt + good, at: ContinuousClock().now))
            XCTAssertEqual(decoder.bufferedBytes, 0)
        }
    }
    func testChunkBoundsAndResetDiscardOldFragments() throws {
        var decoder = GaugeStreamReassembler(); let now = ContinuousClock().now
        XCTAssertThrowsError(try decoder.feed(Data(), at: now))
        XCTAssertThrowsError(try decoder.feed(Data(repeating: 1, count: 513), at: now))
        _ = try decoder.feed(Data([0x41, 0x47, 1]), at: now)
        decoder.reset()
        let frame = GaugeFrame(kind: .event, requestID: 0, payload: Data())
        XCTAssertEqual(try decoder.feed(GaugeFrameCodec.encode(frame), at: now), [frame])
    }
}

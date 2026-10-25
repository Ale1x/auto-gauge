import Foundation
import XCTest
@testable import GaugeCore

final class OTAPacketTransportTests: XCTestCase {
    private func payload() throws -> Data { try OTATransferCodec.encodeRequest(.init(opcode: .start, transferID: 42, offset: 100)) }
    private func expectFailure(_ expected: OTATransferError? = nil, _ operation: () async throws -> Void) async {
        do { try await operation(); XCTFail("Expected failure") }
        catch { if let expected { XCTAssertEqual(error as? OTATransferError, expected) } }
    }
    private func awaitFlag(_ predicate: () async -> Bool) async throws {
        let deadline = ContinuousClock().now.advanced(by: .seconds(2))
        while !(await predicate()) {
            guard ContinuousClock().now < deadline else { throw OTATransferError.timeout }
            try await Task.sleep(for: .milliseconds(1))
        }
    }
    func testClosedGateAndInvalidReadinessNeverWrite() async throws {
        let pipe = OTAPacketMockPipe(); let closed = OTAPacketTransport(pipe: pipe)
        await expectFailure(.unqualified) { try await closed.open() }
        let count = await pipe.readyCalls; XCTAssertEqual(count, 0)
        for (generation, maximum) in [(UInt64(0), 17), (UInt64(1), 0)] {
            let invalidPipe = OTAPacketMockPipe(maximumWriteBytes: maximum, generation: generation)
            let transport = OTAPacketTransport(pipe: invalidPipe, allowHostDevelopmentMutations: true)
            await expectFailure(.notReady) { try await transport.open() }
            let writes = await invalidPipe.writes; XCTAssertTrue(writes.isEmpty)
        }
        let invalidTimeout = OTAPacketTransport(pipe: pipe, allowHostDevelopmentMutations: true, frameTimeout: .zero)
        await expectFailure(.invalidTimeout) { try await invalidTimeout.open() }
    }
    func testExpectedLeaseMismatchNeverDisconnectsNewerConnection() async throws {
        let pipe = OTAPacketMockPipe(generation: 52)
        let transport = OTAPacketTransport(pipe: pipe, allowHostDevelopmentMutations: true)
        await expectFailure(.generationChanged) {
            try await transport.open(expectedConnection: .init(generation: 51, maximumWriteBytes: 17))
        }
        let disconnects = await pipe.disconnects; XCTAssertEqual(disconnects, [51])
        let writes = await pipe.writes; XCTAssertTrue(writes.isEmpty)
    }
    func testFragmentedExchangeUsesMonotonicIDsAndActualWriteLimit() async throws {
        let pipe = OTAPacketMockPipe(maximumWriteBytes: 7, responseFragmentBytes: 1)
        let transport = OTAPacketTransport(pipe: pipe, allowHostDevelopmentMutations: true)
        try await transport.open()
        for _ in 0..<2 {
            let response = try await transport.exchange(payload(), generation: 51, deadline: .now.advanced(by: .seconds(2)))
            XCTAssertEqual(try OTATransferCodec.decodeResponse(response).transferID, 42)
        }
        let writes = await pipe.writes; let requests = await pipe.requests
        XCTAssertTrue(writes.allSatisfy { !$0.isEmpty && $0.count <= 7 })
        XCTAssertEqual(requests.map(\.requestID), [1, 2])
        let begins = await pipe.begins; let ends = await pipe.ends
        XCTAssertEqual(begins, 2); XCTAssertEqual(ends, 2)
        await transport.close()
        await expectFailure(.notReady) { try await transport.open() }
        let disconnected = await pipe.disconnects; XCTAssertEqual(disconnected, [51])
    }
    func testWriteFragmentsNeverExceed512AndFrameTimeoutStartsAtFirstByte() async throws {
        let large = OTAPacketMockPipe(maximumWriteBytes: 4096, responseFragmentBytes: 512)
        let transport = OTAPacketTransport(pipe: large, allowHostDevelopmentMutations: true)
        try await transport.open()
        let bytes = try OTATransferCodec.encodeRequest(.init(opcode: .image, transferID: 7, offset: 0, body: Data(repeating: 0xa5, count: 1024)))
        _ = try await transport.exchange(bytes, generation: 51, deadline: .now.advanced(by: .seconds(2)))
        let sizes = await large.writes.map(\.count); XCTAssertEqual(sizes, [512, 512, 28])
        await transport.close()
        let partial = OTAPacketMockPipe(fault: .partialStall, responseFragmentBytes: 1)
        let partialTransport = OTAPacketTransport(pipe: partial, allowHostDevelopmentMutations: true, frameTimeout: .milliseconds(15))
        try await partialTransport.open()
        let start = ContinuousClock().now
        await expectFailure(.timeout) { _ = try await partialTransport.exchange(self.payload(), generation: 51, deadline: .now.advanced(by: .seconds(2))) }
        XCTAssertLessThan(start.duration(to: .now), .milliseconds(500))
        let disconnects = await partial.disconnects; XCTAssertEqual(disconnects, [51])
    }
    func testInvalidResponseCorruptionTimestampGenerationAndExtraBytesSealSession() async throws {
        for fault in [OTAPacketMockPipe.Fault.wrongID, .event, .extraFrame, .trailingPrefix, .crc, .payload,
                      .futureTime, .staleTime, .generationAfterWrite, .maximumAfterWrite, .emptyChunk] {
            let pipe = OTAPacketMockPipe(fault: fault)
            let transport = OTAPacketTransport(pipe: pipe, allowHostDevelopmentMutations: true)
            try await transport.open()
            await expectFailure { _ = try await transport.exchange(self.payload(), generation: 51, deadline: .now.advanced(by: .seconds(2))) }
            let writesBefore = await pipe.writes.count
            await expectFailure(.notReady) { _ = try await transport.exchange(self.payload(), generation: 51, deadline: .now.advanced(by: .seconds(2))) }
            let writesAfter = await pipe.writes.count; let disconnects = await pipe.disconnects
            XCTAssertEqual(writesBefore, writesAfter, String(describing: fault)); XCTAssertEqual(disconnects, [51])
        }
    }
    func testDeadlinesCancellationAndWrongArgumentCloseWithoutRetry() async throws {
        for fault in [OTAPacketMockPipe.Fault.stalledReceive, .stalledWrite] {
            let pipe = OTAPacketMockPipe(fault: fault)
            let transport = OTAPacketTransport(pipe: pipe, allowHostDevelopmentMutations: true)
            try await transport.open()
            await expectFailure(.timeout) { _ = try await transport.exchange(self.payload(), generation: 51, deadline: .now.advanced(by: .milliseconds(15))) }
            let disconnects = await pipe.disconnects; XCTAssertEqual(disconnects, [51])
        }
        let pipe = OTAPacketMockPipe(fault: .stalledReceive)
        let transport = OTAPacketTransport(pipe: pipe, allowHostDevelopmentMutations: true)
        try await transport.open(); let bytes = try payload()
        let task = Task { try await transport.exchange(bytes, generation: 51, deadline: .now.advanced(by: .seconds(2))) }
        try await awaitFlag { await pipe.receiveEntered }; task.cancel()
        await expectFailure { _ = try await task.value }
        let disconnects = await pipe.disconnects; XCTAssertEqual(disconnects, [51])
        let anotherPipe = OTAPacketMockPipe(); let another = OTAPacketTransport(pipe: anotherPipe, allowHostDevelopmentMutations: true)
        try await another.open()
        await expectFailure(.generationChanged) { _ = try await another.exchange(self.payload(), generation: 52, deadline: .now.advanced(by: .seconds(2))) }
        let writes = await anotherPipe.writes; XCTAssertTrue(writes.isEmpty)
    }
    func testConcurrentExchangeIsBusyButGenerationObservationDoesNotDisrupt() async throws {
        let pipe = OTAPacketMockPipe(fault: .slowReceive, responseFragmentBytes: 512)
        let transport = OTAPacketTransport(pipe: pipe, allowHostDevelopmentMutations: true)
        try await transport.open(); let bytes = try payload()
        let first = Task { try await transport.exchange(bytes, generation: 51, deadline: .now.advanced(by: .seconds(2))) }
        try await awaitFlag { await pipe.receiveEntered }
        await expectFailure(.busy) { _ = try await transport.exchange(bytes, generation: 51, deadline: .now.advanced(by: .seconds(2))) }
        let observed = try await transport.currentGeneration(); XCTAssertEqual(observed, 51)
        _ = try await first.value
        let disconnects = await pipe.disconnects; XCTAssertTrue(disconnects.isEmpty)
        await transport.close()
    }
    func testCloseDuringWriteOrReceiveRejectsLateSuccess() async throws {
        for fault in [OTAPacketMockPipe.Fault.slowWrite, .slowReceive] {
            let pipe = OTAPacketMockPipe(fault: fault, maximumWriteBytes: 512, responseFragmentBytes: 512)
            let transport = OTAPacketTransport(pipe: pipe, allowHostDevelopmentMutations: true)
            try await transport.open(); let bytes = try payload()
            let task = Task { try await transport.exchange(bytes, generation: 51, deadline: .now.advanced(by: .seconds(2))) }
            if fault == .slowWrite { try await awaitFlag { await pipe.writeEntered } }
            else { try await awaitFlag { await pipe.receiveEntered } }
            await transport.close()
            await expectFailure { _ = try await task.value }
            let disconnects = await pipe.disconnects; XCTAssertEqual(disconnects, [51])
        }
    }
    func testCloseDuringOpenOrReadyObservationNeverPublishesConnection() async throws {
        for pauseAt in [1, 2] {
            let pipe = OTAPacketMockPipe(); await pipe.pauseReady(at: pauseAt)
            let transport = OTAPacketTransport(pipe: pipe, allowHostDevelopmentMutations: true)
            if pauseAt == 2 { try await transport.open() }
            let operation = Task { if pauseAt == 1 { try await transport.open() } else { _ = try await transport.currentGeneration() } }
            try await awaitFlag { await pipe.readyCalls >= pauseAt }
            await transport.close(); await expectFailure { try await operation.value }
            await expectFailure(.notReady) { _ = try await transport.currentGeneration() }
            let disconnects = await pipe.disconnects; XCTAssertEqual(disconnects, [51])
        }
    }
}

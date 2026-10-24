import Foundation
import XCTest
@testable import GaugeCore

private enum PipeFault { case none, noReply, partialFrame, corruptCRC, wrongID, wrongKind, wrongPhase, wrongProtocol, receivedOnly, missingReceived, wrongRevision, finalACKLost, earlyApplied, firstWriteFailure, ioAfterReceived, precommitRejected }
private actor FakePacketPipe: GaugePacketPipe {
    var generation: UInt64 = 1
    var ready = true
    let maximumWrite: Int
    var fault: PipeFault
    var currentRevision = 1
    var wire = Data()
    var queue: [GaugeIncomingChunk] = []
    var waiter: CheckedContinuation<GaugeIncomingChunk, any Error>?
    var writeCounts: [Int] = []
    var requestIDs: [UInt32] = []
    var disconnected = false
    var committed = false
    var receiveStarted = false
    init(maximumWrite: Int = 20, fault: PipeFault = .none) { self.maximumWrite = maximumWrite; self.fault = fault }
    func readyConnection() async throws -> GaugePipeConnection {
        guard ready else { throw GaugeLinkError.notReady }; return .init(generation: generation, maximumWriteBytes: maximumWrite)
    }
    func setFault(_ value: PipeFault) { fault = value }
    func setReady(_ value: Bool) { ready = value }
    func reconnect(newGeneration: Bool = true) {
        if newGeneration { generation += 1 }; ready = true; disconnected = false; wire = Data(); queue = []
    }
    func write(_ data: Data, generation lease: UInt64) async throws {
        guard ready, lease == generation else { throw GaugeLinkError.disconnected }
        guard data.count <= min(maximumWrite, 512) else { throw GaugeLinkError.invalidChunk }
        writeCounts.append(data.count)
        if fault == .firstWriteFailure { throw GaugeLinkError.disconnected }
        wire.append(data)
        guard wire.count >= 12 else { return }
        let bytes = Array(wire.prefix(12)); let length = Int(bytes[8]) | Int(bytes[9]) << 8
        guard wire.count == length + 16 else { return }
        let frame = try GaugeFrameCodec.decode(wire); wire = Data(); requestIDs.append(frame.requestID)
        let request = try JSONDecoder().decode(GaugeEndpointRequest.self, from: frame.payload)
        if fault == .noReply { return }
        if fault == .partialFrame { push(Data([0x41, 0x47, 1, 2])); return }
        if request.op == "config.apply" {
            if fault == .precommitRejected {
                push(try response(["op": request.op, "phase": "rejected", "error": "revision_conflict"], id: frame.requestID)); return
            }
            committed = fault != .receivedOnly
            if committed { currentRevision = request.configuration?.revision ?? 2 }
            if fault == .finalACKLost { throw GaugeLinkError.disconnected }
            var packets = Data()
            if fault != .missingReceived { packets += try response(["op": request.op, "phase": "received"], id: frame.requestID) }
            if fault == .ioAfterReceived { packets += try response(["op": request.op, "phase": "rejected", "error": "io_error"], id: frame.requestID) }
            else if fault != .receivedOnly { packets += try response(["op": request.op, "phase": "applied", "applied_revision": fault == .wrongRevision ? 99 : currentRevision, "persistence": "volatile"], id: frame.requestID) }
            push(packets)
            if fault == .earlyApplied { await Task.yield() }
        } else if request.op == "config.get" {
            var config = DemoData.configuration; config.revision = currentRevision
            let object = try JSONSerialization.jsonObject(with: JSONEncoder().encode(config))
            push(try response(["op": request.op, "phase": "current", "configuration": object, "persistence": "volatile"], id: frame.requestID))
        } else {
            var packet = try response(["op": request.op, "protocol_version": fault == .wrongProtocol ? 2 : 1, "simulation": true, "authenticated": true, "supported_service01_pids": [4, 5, 12, 13, 15, 16, 17]], id: fault == .wrongID ? frame.requestID + 1 : frame.requestID)
            if fault == .wrongKind { packet = try GaugeFrameCodec.encode(.init(kind: .event, requestID: 0, payload: Data("{}".utf8))) }
            if fault == .wrongPhase { packet = try response(["op": request.op, "protocol_version": 1, "phase": "current"], id: frame.requestID) }
            if fault == .corruptCRC { packet[packet.count - 1] ^= 1 }
            push(packet)
        }
    }
    private func response(_ object: [String: Any], id: UInt32) throws -> Data {
        try GaugeFrameCodec.encode(.init(kind: .response, requestID: id, payload: JSONSerialization.data(withJSONObject: object, options: .sortedKeys)))
    }
    private func push(_ data: Data) {
        // Also covers two replies coalesced into a single indication before final ATT callback.
        for offset in stride(from: 0, to: data.count, by: 512) { queue.append(.init(data: data.subdata(in: offset..<min(data.count, offset + 512)))) }
        if let waiter, !queue.isEmpty { self.waiter = nil; waiter.resume(returning: queue.removeFirst()) }
    }
    func receive(generation lease: UInt64) async throws -> GaugeIncomingChunk {
        guard ready, lease == generation else { throw GaugeLinkError.disconnected }
        receiveStarted = true
        if !queue.isEmpty { return queue.removeFirst() }
        return try await withTaskCancellationHandler {
            try Task.checkCancellation()
            return try await withCheckedThrowingContinuation { waiter = $0 }
        } onCancel: { Task { await self.cancelRead(generation: lease) } }
    }
    private func cancelRead(generation lease: UInt64) {
        guard lease == generation else { return }
        let old = waiter; waiter = nil; old?.resume(throwing: CancellationError())
    }
    func disconnect(generation lease: UInt64) async {
        guard lease == generation else { return }
        disconnected = true; ready = false; wire = Data(); queue = []
        let old = waiter; waiter = nil; old?.resume(throwing: GaugeLinkError.disconnected)
    }
}

final class BluetoothRPCTests: XCTestCase {
    private let hello = Data("{\"op\":\"hello\"}".utf8)
    private func applyBody(revision: Int = 2, expected: Int = 1) throws -> Data {
        var configuration = DemoData.configuration; configuration.revision = revision
        return try JSONEncoder().encode(GaugeEndpointRequest(op: "config.apply", configuration: configuration, expectedRevision: expected))
    }
    func testFragmentationUsesActualWriteLimitAndRequestIDsIncrease() async throws {
        for limit in [1, 20, 185, 512, 1000] {
            let pipe = FakePacketPipe(maximumWrite: limit); let client = GaugeRPCClient(pipe: pipe)
            try await client.open()
            _ = try await client.request(.hello, body: hello)
            _ = try await client.request(.hello, body: hello)
            let counts = await pipe.writeCounts; XCTAssertTrue(counts.allSatisfy { $0 <= min(limit, 512) })
            let ids = await pipe.requestIDs; XCTAssertEqual(ids, [1, 2])
            do { try await client.open(); XCTFail("Same link must not reset IDs") } catch { }
        }
    }
    func testCCCDReadinessAndZeroWriteLimitPreventRequests() async throws {
        let pipe = FakePacketPipe(); await pipe.setReady(false); let client = GaugeRPCClient(pipe: pipe)
        do { try await client.open(); XCTFail() } catch { }
        let counts = await pipe.writeCounts; XCTAssertTrue(counts.isEmpty)
        let zero = GaugeRPCClient(pipe: FakePacketPipe(maximumWrite: 0))
        do { try await zero.open(); XCTFail() } catch { }
    }
    func testNativeMutationGateDoesNotWriteEvenWhenHelloClaimsAuthentication() async throws {
        let pipe = FakePacketPipe(); let client = GaugeRPCClient(pipe: pipe); try await client.open()
        let helloReply = try await client.request(.hello, body: hello)
        XCTAssertEqual(helloReply.authenticated, true)
        let previousWrites = await pipe.writeCounts
        do { _ = try await client.request(.apply, body: applyBody(), expectedAppliedRevision: 2); XCTFail() }
        catch { guard case GaugeLinkError.unqualified = error else { return XCTFail("\(error)") } }
        let counts = await pipe.writeCounts; XCTAssertEqual(counts, previousWrites)
    }
    func testCRCWrongIDKindPhaseAndProtocolAbortAndDisconnect() async throws {
        for fault in [PipeFault.corruptCRC, .wrongID, .wrongKind, .wrongPhase, .wrongProtocol] {
            let pipe = FakePacketPipe(fault: fault); let client = GaugeRPCClient(pipe: pipe); try await client.open()
            do { _ = try await client.request(.hello, body: hello); XCTFail("\(fault)") } catch { }
            let disconnected = await pipe.disconnected; XCTAssertTrue(disconnected)
        }
    }
    func testReceivedThenAppliedIncludingBeforeLastATTCallback() async throws {
        for fault in [PipeFault.none, .earlyApplied] {
            let pipe = FakePacketPipe(fault: fault); let client = GaugeRPCClient(pipe: pipe, allowTestMutations: true)
            try await client.open()
            let reply = try await client.request(.apply, body: applyBody(), expectedAppliedRevision: 2)
            XCTAssertEqual(reply.phase, "applied"); XCTAssertEqual(reply.appliedRevision, 2)
            let unknown = await client.applyOutcomeUnknown; XCTAssertFalse(unknown)
        }
    }
    func testCommittedBeforeLostFinalWriteACKIsUnknown() async throws {
        let pipe = FakePacketPipe(fault: .finalACKLost); let client = GaugeRPCClient(pipe: pipe, allowTestMutations: true)
        try await client.open()
        do { _ = try await client.request(.apply, body: applyBody(), expectedAppliedRevision: 2); XCTFail() }
        catch { guard case GaugeLinkError.unknownCommit = error else { return XCTFail("\(error)") } }
        let committed = await pipe.committed; let unknown = await client.applyOutcomeUnknown
        XCTAssertTrue(committed); XCTAssertTrue(unknown)
        await pipe.reconnect(); try await client.open()
        let stillUnknown = await client.applyOutcomeUnknown; XCTAssertTrue(stillUnknown)
        let beforeRetry = await pipe.writeCounts
        do { _ = try await client.request(.apply, body: applyBody(), expectedAppliedRevision: 2); XCTFail("Blind retry") } catch { }
        let afterRetry = await pipe.writeCounts; XCTAssertEqual(beforeRetry, afterRetry)
        _ = try await client.request(.hello, body: hello)
        let current = try await client.request(.configuration, body: Data("{\"op\":\"config.get\"}".utf8))
        XCTAssertEqual(current.configuration?.revision, 2)
        XCTAssertTrue(try XCTUnwrap(current.configuration).dashboards.flatMap(\.widgets).allSatisfy { $0.pid.ecuSource == "unknown" })
        let reread = await client.applyOutcomeUnknown; XCTAssertFalse(reread)
        await pipe.setFault(.none)
        let next = try await client.request(.apply, body: applyBody(revision: 3, expected: 2), expectedAppliedRevision: 3)
        XCTAssertEqual(next.appliedRevision, 3)
    }
    func testIOErrorAfterReceivedRemainsUnknownButKnownPrecommitRejectDoesNot() async throws {
        for fault in [PipeFault.ioAfterReceived, .precommitRejected] {
            let pipe = FakePacketPipe(fault: fault); let client = GaugeRPCClient(pipe: pipe, allowTestMutations: true)
            try await client.open()
            do { _ = try await client.request(.apply, body: applyBody(), expectedAppliedRevision: 2); XCTFail() } catch { }
            let unknown = await client.applyOutcomeUnknown
            XCTAssertEqual(unknown, fault == .ioAfterReceived)
        }
    }
    func testWrongAppliedRevisionMissingReceivedAndReceivedOnlyAreNeverSuccess() async throws {
        for fault in [PipeFault.wrongRevision, .missingReceived, .receivedOnly] {
            let pipe = FakePacketPipe(fault: fault)
            let client = GaugeRPCClient(pipe: pipe, allowTestMutations: true, requestTimeout: .milliseconds(200), frameTimeout: .milliseconds(20))
            try await client.open()
            do { _ = try await client.request(.apply, body: applyBody(), expectedAppliedRevision: 2); XCTFail() }
            catch { guard case GaugeLinkError.unknownCommit = error else { return XCTFail("\(error)") } }
        }
    }
    func testFailureBeforeFinalChunkDoesNotClaimApplication() async throws {
        let pipe = FakePacketPipe(fault: .firstWriteFailure); let client = GaugeRPCClient(pipe: pipe, allowTestMutations: true)
        try await client.open()
        do { _ = try await client.request(.apply, body: applyBody(), expectedAppliedRevision: 2); XCTFail() } catch { }
        let unknown = await client.applyOutcomeUnknown; let committed = await pipe.committed
        XCTAssertFalse(unknown); XCTAssertFalse(committed)
    }
    func testAbsoluteRequestAndFrameTimersFireWithoutNewBytes() async throws {
        for fault in [PipeFault.noReply, .partialFrame] {
            let pipe = FakePacketPipe(fault: fault)
            let client = GaugeRPCClient(pipe: pipe, requestTimeout: .milliseconds(40), frameTimeout: .milliseconds(10)); try await client.open()
            do { _ = try await client.request(.hello, body: hello); XCTFail() } catch { }
            let disconnected = await pipe.disconnected; XCTAssertTrue(disconnected)
        }
    }
    func testSingleInFlightCancelDisconnectAndStaleGenerationCannotResume() async throws {
        let pipe = FakePacketPipe(fault: .noReply); let client = GaugeRPCClient(pipe: pipe); try await client.open()
        let first = Task { try await client.request(.hello, body: self.hello) }
        while !(await pipe.receiveStarted) { await Task.yield() }
        do { _ = try await client.request(.hello, body: hello); XCTFail() }
        catch { guard case GaugeLinkError.busy = error else { return XCTFail("\(error)") } }
        await client.disconnect()
        do { _ = try await first.value; XCTFail() } catch { }
        await pipe.reconnect(newGeneration: false)
        do { try await client.open(); XCTFail("Reusing old generation") } catch { }
        await pipe.reconnect(); try await client.open()
        let task = Task { try await client.request(.hello, body: self.hello) }
        while !(await pipe.receiveStarted) { await Task.yield() }
        task.cancel()
        do { _ = try await task.value; XCTFail() } catch { }
    }
    func testBodyOperationMismatchAndOverflowRevisionDoNotSend() async throws {
        let pipe = FakePacketPipe(); let client = GaugeRPCClient(pipe: pipe, allowTestMutations: true); try await client.open()
        do { _ = try await client.request(.hello, body: Data("{\"op\":\"config.apply\"}".utf8)); XCTFail() } catch { }
        do { _ = try await client.request(.apply, body: applyBody(), expectedAppliedRevision: Int.max); XCTFail() } catch { }
        let writes = await pipe.writeCounts; XCTAssertTrue(writes.isEmpty)
    }
}

import Foundation
import XCTest
@testable import GaugeCore

private actor FaultPacketChannel: SimulatedPacketChannel {
    enum Fault: Sendable { case none, corruptFirst, wrongRequestID, wrongPhase, wrongVersion, wrongKind, disconnectBeforeCommit, loseApplied, wrongAppliedRevision }
    let endpoint: InMemoryGaugeEndpoint
    var fault = Fault.none
    var currentLease: SimulatedConnectionLease?
    var operations: [String] = []
    init(_ endpoint: InMemoryGaugeEndpoint) { self.endpoint = endpoint }
    func setFault(_ value: Fault) { fault = value }
    func sentOperations() -> [String] { operations }
    func connect(clientID: String) async throws -> SimulatedConnectionLease {
        let lease = try await endpoint.connect(clientID: clientID); currentLease = lease; return lease
    }
    func disconnect(lease: SimulatedConnectionLease) async { await endpoint.disconnect(lease: lease) }
    func begin(packet: Data, lease: SimulatedConnectionLease) async throws -> SimulatedReplyBatch {
        let request = try GaugeFrameCodec.decode(packet)
        let parsed = try JSONDecoder().decode(GaugeEndpointRequest.self, from: request.payload)
        operations.append(parsed.op)
        let batch = try await endpoint.begin(packet: packet, lease: lease)
        guard let packet = batch.packets.first else { return batch }
        switch fault {
        case .corruptFirst:
            fault = .none
            var bad = packet; bad[bad.count-1] ^= 1
            return .init(packets: [bad], hasContinuation: batch.hasContinuation)
        case .wrongRequestID, .wrongKind, .wrongPhase, .wrongVersion:
            let value = fault; fault = .none
            var frame = try GaugeFrameCodec.decode(packet)
            if value == .wrongRequestID { frame.requestID += 1 }
            if value == .wrongKind { frame.kind = .request }
            if value == .wrongPhase || value == .wrongVersion {
                var reply = try JSONDecoder().decode(GaugeEndpointReply.self, from: frame.payload)
                if value == .wrongPhase { reply.phase = "validated" }
                else { reply.protocolVersion = 2 }
                frame.payload = try JSONEncoder().encode(reply)
            }
            return .init(packets: [try GaugeFrameCodec.encode(frame)], hasContinuation: batch.hasContinuation)
        default: return batch
        }
    }
    func finish(requestID: UInt32, lease: SimulatedConnectionLease) async throws -> [Data] {
        if fault == .disconnectBeforeCommit {
            fault = .none; await endpoint.disconnect(lease: lease); throw SimulatedLinkError.disconnected
        }
        let replies = try await endpoint.finish(requestID: requestID, lease: lease)
        if fault == .loseApplied { fault = .none; return [] }
        if fault == .wrongAppliedRevision, let packet = replies.first {
            fault = .none
            var frame = try GaugeFrameCodec.decode(packet)
            var reply = try JSONDecoder().decode(GaugeEndpointReply.self, from: frame.payload)
            reply.appliedRevision = 123
            frame.payload = try JSONEncoder().encode(reply)
            return [try GaugeFrameCodec.encode(frame)]
        }
        return replies
    }
    func applyOutsideClientCache(_ configuration: GaugeConfiguration) async throws {
        guard let lease = currentLease else { throw SimulatedLinkError.disconnected }
        let request = GaugeEndpointRequest(op: "config.apply", configuration: configuration, expectedRevision: configuration.revision-1)
        let packet = try GaugeFrameCodec.encode(.init(kind: .request, requestID: 30000, payload: JSONEncoder().encode(request)))
        _ = try await endpoint.begin(packet: packet, lease: lease)
        _ = try await endpoint.finish(requestID: 30000, lease: lease)
    }
}

private final class RPCLogStorage: LogArchiveStorage, @unchecked Sendable {
    var data: Data?
    var failWrite = false
    func read(maximumBytes: Int) -> Data? { data }
    func write(_ data: Data) throws { if failWrite { throw LogImportError.capacity }; self.data = data }
}

final class SimulatedRoundTripTests: XCTestCase {
    private func endpoint() throws -> InMemoryGaugeEndpoint {
        let fixture = try DemoGaugeFixture.load()
        return try .init(configuration: fixture.configuration, historySource: fixture.historySource)
    }
    private func setup() async throws -> (InMemoryGaugeEndpoint, FaultPacketChannel, SimulatedGaugeTransport) {
        let endpoint = try endpoint(), link = FaultPacketChannel(endpoint)
        let client = SimulatedGaugeTransport(channel: link, clientID: "A")
        try await client.connect()
        _ = try await client.configuration()
        return (endpoint, link, client)
    }
    func testGoldenExchangesMatchSharedJSONSemantically() async throws {
        let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
        let object = try XCTUnwrap(JSONSerialization.jsonObject(with: Data(contentsOf: root.appendingPathComponent("shared/fixtures/rpc.exchanges.json"))) as? [String: Any])
        let initial = try JSONDecoder().decode(GaugeConfiguration.self, from: JSONSerialization.data(withJSONObject: XCTUnwrap(object["initial_configuration"])))
        let fixture = try DemoGaugeFixture.load()
        XCTAssertEqual(initial, fixture.configuration)
        let device = try InMemoryGaugeEndpoint(configuration: initial, historySource: fixture.historySource)
        let lease = try await device.connect(clientID: "golden")
        for exchange in try XCTUnwrap(object["exchanges"] as? [[String: Any]]) {
            let id = try XCTUnwrap(exchange["request_id"] as? NSNumber).uint32Value
            let request = try JSONSerialization.data(withJSONObject: XCTUnwrap(exchange["request"]))
            let packet = try GaugeFrameCodec.encode(.init(kind: .request, requestID: id, payload: request))
            let batch = try await device.begin(packet: packet, lease: lease)
            var packets = batch.packets
            if batch.hasContinuation { packets += try await device.finish(requestID: id, lease: lease) }
            let expected = try XCTUnwrap(exchange["responses"] as? [NSDictionary])
            XCTAssertEqual(packets.count, expected.count)
            for (packet, expected) in zip(packets, expected) {
                let frame = try GaugeFrameCodec.decode(packet)
                XCTAssertEqual(frame.requestID, id)
                XCTAssertEqual(frame.kind, .response)
                XCTAssertEqual(try JSONSerialization.jsonObject(with: frame.payload) as? NSDictionary, expected, "request \(id)")
            }
        }
    }
    func testRoundTripReadValidateApplyKeepsPhoneRevisionDistinct() async throws {
        let (_, _, client) = try await setup()
        var draft = DemoData.configuration
        draft.revision = 700
        draft.brightness.manualPercent = 19
        try await client.validateDraft(draft)
        let applied = try await client.applyDraft(draft)
        XCTAssertEqual(applied.revision, 2)
        XCTAssertEqual(draft.revision, 700)
        let phases = await client.lastApplyPhases(), state = await client.commitState()
        XCTAssertEqual(phases, ["received", "applied"])
        XCTAssertEqual(state, .appliedVolatile(2))
        let readback = try await client.configuration()
        XCTAssertEqual(readback, applied)
    }
    func testCorruptedWrongIDKindAndPhaseRepliesPreserveSnapshots() async throws {
        for fault in [FaultPacketChannel.Fault.corruptFirst, .wrongRequestID, .wrongKind, .wrongPhase] {
            let (endpoint, channel, client) = try await setup()
            let before = await endpoint.simulatedConfiguration(), cached = await client.lastObservedConfiguration()
            await channel.setFault(fault)
            do { _ = try await client.configuration(); XCTFail("Malformed reply accepted") } catch { }
            let after = await endpoint.simulatedConfiguration(), observed = await client.lastObservedConfiguration()
            XCTAssertEqual(after, before)
            XCTAssertEqual(observed, cached)
            let refresh = await client.requiresRead(); XCTAssertTrue(refresh)
        }
    }
    func testMalformedReceivedPhaseNeverCommits() async throws {
        let (endpoint, channel, client) = try await setup()
        let before = await endpoint.simulatedConfiguration()
        await channel.setFault(.wrongPhase)
        do { _ = try await client.applyDraft(DemoData.configuration); XCTFail("Wrong received phase accepted") } catch { }
        let after = await endpoint.simulatedConfiguration()
        XCTAssertEqual(after, before)
        let phases = await client.lastApplyPhases()
        XCTAssertFalse(phases.contains("applied"))
    }
    func testProtocolAndFrameVersionsRejected() async throws {
        let endpoint = try endpoint(), channel = FaultPacketChannel(endpoint)
        let client = SimulatedGaugeTransport(channel: channel, clientID: "A")
        try await client.connect(); await channel.setFault(.wrongVersion)
        do { _ = try await client.supportedPIDs(); XCTFail("Protocol2 accepted") } catch { }
        let lease = try await endpoint.connectForTestAfterDisconnect(channel: channel)
        var packet = try GaugeFrameCodec.encode(.init(kind: .request, requestID: 1, payload: Data("{\"op\":\"hello\"}".utf8)))
        packet[2] = 2
        let crc = GaugeFrameCodec.crc32(packet.dropLast(4))
        for i in 0..<4 { packet[packet.count-4+i] = UInt8((crc >> (8*i)) & 0xff) }
        do { _ = try await endpoint.begin(packet: packet, lease: lease); XCTFail("Frame version2 accepted") } catch { }
    }
    func testDisconnectAtReceivedAbortsPendingBeforeCommit() async throws {
        let (endpoint, _, client) = try await setup()
        let before = await endpoint.simulatedConfiguration()
        do {
            _ = try await client.applyDraft(DemoData.configuration, onPhase: { phase in
                if phase == .received { await client.disconnect() }
            })
            XCTFail("Disconnected apply accepted")
        } catch { }
        let after = await endpoint.simulatedConfiguration()
        XCTAssertEqual(after, before)
    }
    func testLostAppliedAckIsUnknownWithoutRollingBackDevice() async throws {
        for fault in [FaultPacketChannel.Fault.loseApplied, .wrongAppliedRevision] {
            let (endpoint, channel, client) = try await setup()
            let cached = await client.lastObservedConfiguration()
            await channel.setFault(fault)
            do { _ = try await client.applyDraft(DemoData.configuration); XCTFail("Lost applied ACK treated as success") } catch { }
            let device = await endpoint.simulatedConfiguration(), observed = await client.lastObservedConfiguration()
            XCTAssertEqual(device.revision, 2)
            XCTAssertEqual(observed, cached)
            let state = await client.commitState(); XCTAssertEqual(state, .unknown)
            try await client.connect()
            let refreshed = try await client.configuration()
            XCTAssertEqual(refreshed.revision, 2)
            let resolved = await client.commitState(); XCTAssertEqual(resolved, .idle)
        }
    }
    func testStaleRevisionRejectsWithoutOverwritingDraftOrDevice() async throws {
        let (endpoint, channel, client) = try await setup()
        var outside = try await client.configuration(); outside.revision = 2; outside.brightness.manualPercent = 81
        try await channel.applyOutsideClientCache(outside)
        var draft = DemoData.configuration; draft.brightness.manualPercent = 22
        let original = draft
        do { _ = try await client.applyDraft(draft); XCTFail("Stale commit accepted") } catch { }
        XCTAssertEqual(draft, original)
        let device = await endpoint.simulatedConfiguration(); XCTAssertEqual(device, outside)
        let phases = await client.lastApplyPhases(); XCTAssertEqual(phases, ["received", "rejected"])
        let refresh = await client.requiresRead(); XCTAssertTrue(refresh)
    }
    func testSequentialClientsAndOldLeaseDoNotRequireResetOrAffectNewClient() async throws {
        let endpoint = try endpoint()
        let a = try await endpoint.connect(clientID: "A")
        do { _ = try await endpoint.connect(clientID: "B"); XCTFail("Two simultaneous clients") } catch { }
        var config = DemoData.configuration; config.revision = 2
        let request = try GaugeFrameCodec.encode(.init(kind: .request, requestID: 1, payload: JSONEncoder().encode(GaugeEndpointRequest(op: "config.apply", configuration: config, expectedRevision: 1))))
        _ = try await endpoint.begin(packet: request, lease: a)
        await endpoint.disconnect(lease: a)
        let b = try await endpoint.connect(clientID: "B")
        do { _ = try await endpoint.finish(requestID: 1, lease: a); XCTFail("Old A pending reached B") } catch { }
        await endpoint.disconnect(lease: a)
        let hello = try GaugeFrameCodec.encode(.init(kind: .request, requestID: 1, payload: Data("{\"op\":\"hello\"}".utf8)))
        _ = try await endpoint.begin(packet: hello, lease: b)
        await endpoint.disconnect(lease: b)
        _ = try await endpoint.connect(clientID: "A")
        let current = await endpoint.simulatedConfiguration(); XCTAssertEqual(current.revision, 1)
    }
    func testLogACKFollowsLocalWriteAndNeverDeletesSourceForOtherClient() async throws {
        let (endpoint, channel, client) = try await setup()
        let fixture = try DemoGaugeFixture.load(), storage = RPCLogStorage()
        let archive = LocalLogArchive(storage: storage); try await archive.reload()
        storage.failWrite = true
        do { _ = try await client.importHistoryPage(sessionID: fixture.historySource.sessionID, archive: archive, limit: 2); XCTFail("ACK after failed store") } catch { }
        var operations = await channel.sentOperations(); XCTAssertFalse(operations.contains("logs.ack"))
        storage.failWrite = false
        try await archive.reload()
        let imported = try await client.importHistoryPage(sessionID: fixture.historySource.sessionID, archive: archive, limit: 2)
        XCTAssertTrue(imported.peerAcknowledged)
        XCTAssertEqual(imported.acknowledgement.acknowledgedThrough.value, 4)
        operations = await channel.sentOperations(); XCTAssertTrue(operations.contains("logs.ack"))
        await client.disconnect()
        let second = SimulatedGaugeTransport(channel: endpoint, clientID: "B")
        try await second.connect()
        let samePage = try await second.readLogs(sessionID: fixture.historySource.sessionID, after: 0, limit: 2)
        XCTAssertEqual(samePage.records.map(\.sequence.value), [3, 4])
        XCTAssertEqual(samePage.lost.first?.last.value, 2)
    }
}

private extension InMemoryGaugeEndpoint {
    func connectForTestAfterDisconnect(channel: FaultPacketChannel) async throws -> SimulatedConnectionLease {
        if let current = await channel.currentLease { disconnect(lease: current) }
        return try connect(clientID: "versions")
    }
}

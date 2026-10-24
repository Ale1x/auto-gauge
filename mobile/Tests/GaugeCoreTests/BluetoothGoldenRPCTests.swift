import Foundation
import XCTest
@testable import GaugeCore

private struct GoldenRPCExchange: Sendable {
    let id: UInt32
    let request: Data
    let responses: [Data]
    var trailingWire = Data()
}

/// Replays fixture payloads without injecting op or normalizing response fields.
private actor GoldenPacketPipe: GaugePacketPipe {
    let exchanges: [GoldenRPCExchange]
    let responseChunkBytes: Int
    var index = 0
    var requestBuffer = Data()
    var chunks: [GaugeIncomingChunk] = []
    var transmittedResponses: [Data] = []
    var disconnectCount = 0
    var ready = true
    var generation: UInt64 = 1
    init(_ exchanges: [GoldenRPCExchange], responseChunkBytes: Int = 17) { self.exchanges = exchanges; self.responseChunkBytes = responseChunkBytes }
    func readyConnection() async throws -> GaugePipeConnection { .init(generation: generation, maximumWriteBytes: 20) }
    func write(_ data: Data, generation: UInt64) async throws {
        guard ready, generation == self.generation, data.count <= 20 else { throw GaugeLinkError.disconnected }
        requestBuffer.append(data)
        guard requestBuffer.count >= 12 else { return }
        let bytes = Array(requestBuffer.prefix(12)); let length = Int(bytes[8]) | Int(bytes[9]) << 8
        guard requestBuffer.count == length + 16 else { return }
        guard index < exchanges.count else { throw GaugeLinkError.invalidReply }
        let request = try GaugeFrameCodec.decode(requestBuffer); requestBuffer = Data()
        let exchange = exchanges[index]
        guard request.requestID == exchange.id,
              try JSONSerialization.jsonObject(with: request.payload) as? NSDictionary == JSONSerialization.jsonObject(with: exchange.request) as? NSDictionary else { throw GaugeLinkError.invalidReply }
        var wire = Data()
        for payload in exchange.responses {
            transmittedResponses.append(payload)
            wire += try GaugeFrameCodec.encode(.init(kind: .response, requestID: exchange.id, payload: payload))
        }
        wire += exchange.trailingWire
        guard wire.count <= 4112 else { throw GaugeLinkError.invalidChunk }
        for offset in stride(from: 0, to: wire.count, by: responseChunkBytes) {
            chunks.append(.init(data: wire.subdata(in: offset..<min(wire.count, offset + responseChunkBytes))))
        }
        index += 1
    }
    func receive(generation: UInt64) async throws -> GaugeIncomingChunk {
        guard ready, generation == self.generation, !chunks.isEmpty else { throw GaugeLinkError.disconnected }
        return chunks.removeFirst()
    }
    func disconnect(generation: UInt64) async {
        guard generation == self.generation else { return }
        ready = false; disconnectCount += 1; chunks = []; requestBuffer = Data()
    }
    func reconnect() { generation += 1; ready = true }
}

final class BluetoothGoldenRPCTests: XCTestCase {
    private func encoded(_ object: Any) throws -> Data { try JSONSerialization.data(withJSONObject: object, options: .sortedKeys) }
    private func fixtures() throws -> [GoldenRPCExchange] {
        let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
        let data = try Data(contentsOf: root.appendingPathComponent("shared/fixtures/rpc.exchanges.json"))
        let object = try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [String: Any])
        return try XCTUnwrap(object["exchanges"] as? [[String: Any]]).map { exchange in
            .init(id: try XCTUnwrap(exchange["request_id"] as? UInt32), request: try encoded(XCTUnwrap(exchange["request"])), responses: try XCTUnwrap(exchange["responses"] as? [Any]).map(encoded))
        }
    }
    func testAllTenOriginalGoldenExchangesEndToEndWithoutAddingOp() async throws {
        let exchanges = try fixtures(); XCTAssertEqual(exchanges.count, 10)
        let pipe = GoldenPacketPipe(exchanges); let client = GaugeRPCClient(pipe: pipe, allowTestMutations: true)
        try await client.open()
        for exchange in exchanges {
            let request = try JSONDecoder().decode(GaugeEndpointRequest.self, from: exchange.request)
            let operation = try XCTUnwrap(GaugeRPCOperation(rawValue: request.op))
            let expected = try JSONDecoder().decode(GaugeRPCResponse.self, from: XCTUnwrap(exchange.responses.last))
            do {
                let actual = try await client.request(operation, body: exchange.request, expectedAppliedRevision: operation == .apply ? request.configuration?.revision : nil)
                XCTAssertNil(expected.error, "exchange \(exchange.id)")
                XCTAssertEqual(actual.op, expected.op) // Legacy omission remains omission.
                XCTAssertEqual(actual.phase, expected.phase); XCTAssertEqual(actual.appliedRevision, expected.appliedRevision)
                XCTAssertEqual(actual.page, expected.page); XCTAssertEqual(actual.acknowledgedThrough, expected.acknowledgedThrough)
                if let actualConfiguration = actual.configuration, let expectedConfiguration = expected.configuration {
                    let actualWire = try JSONSerialization.jsonObject(with: JSONEncoder().encode(actualConfiguration)) as? NSDictionary
                    let expectedWire = try JSONSerialization.jsonObject(with: JSONEncoder().encode(expectedConfiguration)) as? NSDictionary
                    XCTAssertEqual(actualWire, expectedWire)
                }
            } catch {
                let reason = try XCTUnwrap(expected.error, "Unexpected rejection in \(exchange.id): \(error)")
                guard case GaugeLinkError.rejected(let actual) = error else { return XCTFail("\(exchange.id): \(error)") }
                XCTAssertEqual(actual, reason)
            }
        }
        let sent = await pipe.transmittedResponses
        XCTAssertEqual(sent, exchanges.flatMap(\.responses))
        let requests = await pipe.index; XCTAssertEqual(requests, 10)
        let disconnects = await pipe.disconnectCount; XCTAssertEqual(disconnects, 0)
        let unknown = await client.applyOutcomeUnknown; XCTAssertFalse(unknown)
    }
    func testMissingOpOnHelloAndLogsSuccessIsRejected() async throws {
        let exchanges = try fixtures()
        for index in [0, 6] {
            var object = try XCTUnwrap(JSONSerialization.jsonObject(with: XCTUnwrap(exchanges[index].responses.first)) as? [String: Any])
            object.removeValue(forKey: "op")
            let response = try encoded(object)
            let exchange = GoldenRPCExchange(id: 1, request: exchanges[index].request, responses: [response])
            let pipe = GoldenPacketPipe([exchange]); let client = GaugeRPCClient(pipe: pipe); try await client.open()
            let operation = index == 0 ? GaugeRPCOperation.hello : .logs
            do { _ = try await client.request(operation, body: exchange.request); XCTFail() } catch { }
            let disconnects = await pipe.disconnectCount; XCTAssertEqual(disconnects, 1)
        }
    }
    func testPresentNullBooleanNumericOrWrongOpCannotUseLegacyFallback() async throws {
        let exchanges = try fixtures()
        for badOp: Any in [NSNull(), true, 7, "logs.read"] {
            var object = try XCTUnwrap(JSONSerialization.jsonObject(with: exchanges[1].responses[0]) as? [String: Any])
            object["op"] = badOp
            let exchange = GoldenRPCExchange(id: 2, request: exchanges[1].request, responses: [try encoded(object)])
            let pipe = GoldenPacketPipe([exchanges[0], exchange]); let client = GaugeRPCClient(pipe: pipe); try await client.open()
            _ = try await client.request(.hello, body: exchanges[0].request)
            do { _ = try await client.request(.configuration, body: exchange.request); XCTFail("op=\(badOp)") } catch { }
            let disconnects = await pipe.disconnectCount; XCTAssertEqual(disconnects, 1)
        }
    }
    func testLegacyShapesCannotSmuggleUnexpectedFieldsOrUnphasedErrors() async throws {
        let exchanges = try fixtures()
        let invalid: [[String: Any]] = [
            ["phase": "current", "error": "revision_conflict"],
            ["phase": "validated", "protocol_version": 1],
            ["error": "session_mismatch"],
            ["phase": "rejected", "error": "", "authenticated": true]
        ]
        for object in invalid {
            // Use the valid configuration from golden validate; only the response is adversarial.
            let exchange = GoldenRPCExchange(id: 1, request: exchanges[2].request, responses: [try encoded(object)])
            let pipe = GoldenPacketPipe([exchange]); let client = GaugeRPCClient(pipe: pipe); try await client.open()
            do { _ = try await client.request(.validate, body: exchange.request); XCTFail("\(object)") } catch { }
            let disconnects = await pipe.disconnectCount; XCTAssertEqual(disconnects, 1)
        }
    }
    func testRejectedWithExtraFrameOrTrailingPartialClosesInsteadOfKeepingLink() async throws {
        let base = try fixtures()[9]
        let extra = try GaugeFrameCodec.encode(.init(kind: .response, requestID: 1, payload: Data("{\"op\":\"logs.read\"}".utf8)))
        for tail in [Data([0x41]), extra] {
            let exchange = GoldenRPCExchange(id: 1, request: base.request, responses: base.responses, trailingWire: tail)
            let pipe = GoldenPacketPipe([exchange], responseChunkBytes: 512); let client = GaugeRPCClient(pipe: pipe); try await client.open()
            do { _ = try await client.request(.logs, body: exchange.request); XCTFail() } catch { }
            let disconnects = await pipe.disconnectCount; XCTAssertEqual(disconnects, 1)
        }
    }
    func testTypedConfigurationDiagnosticsAcceptOnlyCoherentStoreStates() async throws {
        let exchanges = try fixtures()
        for state in [GaugeEndpointStoreState.readOnly, .capabilityMismatch, .noValidConfiguration, .conflict, .ioOrIntegrityError] {
            for includeOp in [false, true] {
                var object: [String: Any] = ["phase": "diagnostic", "error": "configuration_not_operational", "store_state": state.rawValue]
                if includeOp { object["op"] = "config.get" }
                let get = GoldenRPCExchange(id: 2, request: exchanges[1].request, responses: [try encoded(object)])
                let pipe = GoldenPacketPipe([exchanges[0], get]); let client = GaugeRPCClient(pipe: pipe); try await client.open()
                _ = try await client.request(.hello, body: exchanges[0].request)
                do { _ = try await client.request(.configuration, body: get.request); XCTFail() }
                catch let failure as GaugeEndpointFailure {
                    XCTAssertEqual(failure.kind, .configurationNotOperational); XCTAssertEqual(failure.storeState, state); XCTAssertEqual(failure.operation, .configuration)
                    XCTAssertTrue(failure.localizedDescription.contains("Configurazione non verificabile"))
                    XCTAssertFalse(failure.localizedDescription.contains(state.rawValue))
                    let source = await client.lastEndpointFailure; XCTAssertEqual(source, failure)
                }
            }
        }
    }
    func testTypedHistoryReadOnlyDiagnosticForReadAndACK() async throws {
        let exchanges = try fixtures()
        for index in [6, 7] {
            let operation: GaugeRPCOperation = index == 6 ? .logs : .acknowledge
            let payload = try encoded(["phase": "diagnostic", "error": "history_not_operational", "store_state": "read_only"])
            let exchange = GoldenRPCExchange(id: 1, request: exchanges[index].request, responses: [payload])
            let pipe = GoldenPacketPipe([exchange]); let client = GaugeRPCClient(pipe: pipe, allowTestMutations: true); try await client.open()
            do { _ = try await client.request(operation, body: exchange.request); XCTFail() }
            catch let failure as GaugeEndpointFailure {
                XCTAssertEqual(failure.kind, .historyNotOperational); XCTAssertEqual(failure.storeState, .readOnly)
                XCTAssertFalse(failure.localizedDescription.contains("history_not_operational"))
            }
        }
    }
    func testTypedOutcomeUnknownAndReadOnlyDiagnosticCannotResolveApplyLatch() async throws {
        let exchanges = try fixtures()
        let unknown = try encoded(["phase": "outcome_unknown", "error": "outcome_unknown", "store_state": "commit_or_readback_failed"])
        let diagnostic = try encoded(["phase": "diagnostic", "error": "configuration_not_operational", "store_state": "read_only"])
        let apply = GoldenRPCExchange(id: 1, request: exchanges[3].request, responses: [exchanges[3].responses[0], unknown])
        let get = GoldenRPCExchange(id: 2, request: exchanges[1].request, responses: [diagnostic])
        let pipe = GoldenPacketPipe([apply, exchanges[0], get]); let client = GaugeRPCClient(pipe: pipe, allowTestMutations: true); try await client.open()
        do { _ = try await client.request(.apply, body: apply.request, expectedAppliedRevision: 2); XCTFail() }
        catch { guard case GaugeLinkError.unknownCommit = error else { return XCTFail("\(error)") } }
        let cause = await client.lastEndpointFailure; XCTAssertEqual(cause?.kind, .outcomeUnknown); XCTAssertEqual(cause?.storeState, .commitOrReadbackFailed)
        await pipe.reconnect(); try await client.open(); _ = try await client.request(.hello, body: exchanges[0].request)
        do { _ = try await client.request(.configuration, body: get.request); XCTFail() }
        catch let failure as GaugeEndpointFailure { XCTAssertEqual(failure.storeState, .readOnly) }
        let latched = await client.applyOutcomeUnknown; XCTAssertTrue(latched)
        let before = await pipe.index
        do { _ = try await client.request(.apply, body: apply.request, expectedAppliedRevision: 2); XCTFail() } catch { }
        let after = await pipe.index; XCTAssertEqual(before, after)
    }
    func testDiagnosticWrongPhaseOperationStateAndSuccessFieldsNeverBecomeCurrent() async throws {
        let exchanges = try fixtures()
        let configuration = try XCTUnwrap(JSONSerialization.jsonObject(with: exchanges[1].responses[0]) as? [String: Any])["configuration"]
        let invalid: [[String: Any]] = [
            ["phase": "diagnostic", "error": "configuration_not_operational", "store_state": "commit_or_readback_failed"],
            ["phase": "diagnostic", "error": "configuration_not_operational", "store_state": "unknown"],
            ["phase": "diagnostic", "error": "configuration_not_operational", "store_state": true],
            ["phase": "diagnostic", "error": "history_not_operational", "store_state": "read_only"],
            ["phase": "outcome_unknown", "error": "outcome_unknown", "store_state": "commit_or_readback_failed"],
            ["phase": "diagnostic", "error": "configuration_not_operational", "store_state": "read_only", "op": "logs.read"],
            ["phase": "diagnostic", "error": "configuration_not_operational", "store_state": "read_only", "op": NSNull()],
            ["phase": "diagnostic", "error": "configuration_not_operational", "store_state": "read_only", "persistence": "durable"],
            ["phase": "diagnostic", "error": "configuration_not_operational", "store_state": "read_only", "applied_revision": 2],
            ["phase": "current", "op": "config.get", "configuration": try XCTUnwrap(configuration), "persistence": "volatile", "store_state": "read_only"]
        ]
        for object in invalid {
            let get = GoldenRPCExchange(id: 2, request: exchanges[1].request, responses: [try encoded(object)])
            let pipe = GoldenPacketPipe([exchanges[0], get]); let client = GaugeRPCClient(pipe: pipe); try await client.open()
            _ = try await client.request(.hello, body: exchanges[0].request)
            do { _ = try await client.request(.configuration, body: get.request); XCTFail("\(object)") } catch { }
            let source = await client.lastEndpointFailure; XCTAssertNil(source)
            let count = await pipe.disconnectCount; XCTAssertEqual(count, 1)
        }
    }

}

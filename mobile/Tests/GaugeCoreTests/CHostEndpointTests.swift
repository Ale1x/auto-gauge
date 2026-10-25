#if os(macOS)
import Foundation
import XCTest
@testable import GaugeCore

private actor CHostReceivedFlag {
    var received = false
    func mark() { received = true }
}

final class CHostEndpointTests: XCTestCase {
    override class func tearDown() {
        if case .success(let compiled) = CHostCompiler.result { try? FileManager.default.removeItem(at: compiled.directory) }
        super.tearDown()
    }
    private let session = try? LogSessionID(String(repeating: "05", count: 16))
    private func fixture() async throws -> (CHostEndpointHub, URL) {
        let compiled = try CHostCompiler.result.get()
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent("auto-gauge-c-integration-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        let hub = CHostEndpointHub(executable: compiled.binary, directory: directory)
        addTeardownBlock { await hub.shutdown(); try? FileManager.default.removeItem(at: directory) }
        return (hub, directory)
    }
    private func transport(_ hub: CHostEndpointHub, client: String = "a", timeout: Duration = .seconds(5)) -> DevelopmentGaugeTransport {
        .init(pipe: CHostPacketPipe(hub: hub, client: client), persistencePolicy: .hostDurableStore, requestTimeout: timeout)
    }
    func testCProcessDurableEditorReadbackAndAtomicImportACK() async throws {
        let (hub, directory) = try await fixture(); let transport = transport(hub)
        try await transport.connect()
        let catalog = try await transport.supportedPIDs(); let initial = try await transport.configuration()
        XCTAssertEqual(initial.revision, 1)
        var phone = initial; phone.revision = 40
        var editor = try ConfigurationDraft(configuration: phone, catalog: catalog)
        editor.working.brightness.manualPercent = 72
        try editor.save(to: AtomicFileConfigurationStorage(fileURL: directory.appendingPathComponent("draft.json")), catalog: catalog)
        try await transport.validateDraft(editor.working)
        let applied = try await transport.applyDraft(editor.working)
        XCTAssertEqual(applied.revision, 2); XCTAssertEqual(editor.saved.revision, 41)
        let state = await transport.commitState(); XCTAssertEqual(state, .appliedHostDurable(2))
        let readback = try await transport.configuration(); XCTAssertEqual(readback.revision, 2); XCTAssertEqual(readback.brightness.manualPercent, 72)
        let label = await transport.lastPersistence(); XCTAssertEqual(label, "durable")
        let logFile = directory.appendingPathComponent("history-a.json")
        let archive = LocalLogArchive(storage: AtomicLogFileStorage(fileURL: logFile)); try await archive.reload()
        let imported = try await transport.importHistoryPage(sessionID: XCTUnwrap(session), archive: archive, limit: 2)
        XCTAssertTrue(imported.peerAcknowledged); XCTAssertEqual(imported.acknowledgement.acknowledgedThrough.value, 4)
        let reopened = LocalLogArchive(storage: AtomicLogFileStorage(fileURL: logFile)); try await reopened.reload()
        let rows = await reopened.sessions(); XCTAssertEqual(rows.first?.records.count, 2); XCTAssertEqual(rows.first?.lost.first?.last.value, 2)
        let operations = await hub.operations; XCTAssertEqual(Array(operations.suffix(2)), ["logs.read", "logs.ack"])
        let store = hub.storeFile; XCTAssertEqual(try Data(contentsOf: store).count, 8192)
        await transport.disconnect()
    }
    func testCSequentialABAClientsKeepOneProcessAndIndependentHistoryCursors() async throws {
        let (hub, directory) = try await fixture(); let a = transport(hub); let b = transport(hub, client: "b")
        try await a.connect(); let initial = try await a.configuration()
        let processA = await hub.processIdentifier()
        do { try await b.connect(); XCTFail("Only one active client") } catch { }
        let archiveA = LocalLogArchive(storage: AtomicLogFileStorage(fileURL: directory.appendingPathComponent("a.json"))); try await archiveA.reload()
        let first = try await a.importHistoryPage(sessionID: XCTUnwrap(session), archive: archiveA, limit: 2)
        XCTAssertEqual(first.acknowledgement.acknowledgedThrough.value, 4)
        _ = try await a.applyDraft(initial); await a.disconnect()
        try await b.connect(); let bConfig = try await b.configuration(); XCTAssertEqual(bConfig.revision, 2)
        let archiveB = LocalLogArchive(storage: AtomicLogFileStorage(fileURL: directory.appendingPathComponent("b.json"))); try await archiveB.reload()
        let second = try await b.importHistoryPage(sessionID: XCTUnwrap(session), archive: archiveB, limit: 2)
        XCTAssertEqual(second.acknowledgement.acknowledgedThrough.value, 4) // A ACK did not erase the C ring.
        var changed = bConfig; changed.brightness.manualPercent = 81
        _ = try await b.applyDraft(changed); await b.disconnect()
        try await a.connect()
        let before = await a.lastObservedConfiguration(); XCTAssertEqual(before?.revision, 2)
        do { _ = try await a.applyDraft(changed); XCTFail("Stale cache must require read") } catch { }
        let current = try await a.configuration(); XCTAssertEqual(current.revision, 3)
        let tail = try await a.importHistoryPage(sessionID: XCTUnwrap(session), archive: archiveA, limit: 2)
        XCTAssertEqual(tail.acknowledgement.acknowledgedThrough.value, 6)
        let rowsA = await archiveA.sessions(); let rowsB = await archiveB.sessions()
        XCTAssertEqual(rowsA.first?.records.count, 4); XCTAssertEqual(rowsB.first?.records.count, 2)
        let processB = await hub.processIdentifier(); XCTAssertEqual(processA, processB)
        await a.disconnect()
    }
    func testCLostAppliedACKRequiresReadbackAfterProcessRestart() async throws {
        let (hub, _) = try await fixture(); let transport = transport(hub, timeout: .seconds(2))
        try await transport.connect(); let initial = try await transport.configuration()
        let firstPID = await hub.processIdentifier()
        try await hub.command("drop-applied")
        do { _ = try await transport.applyDraft(initial); XCTFail() } catch { }
        let unknown = await transport.commitState(); XCTAssertEqual(unknown, .unknown)
        let cached = await transport.lastObservedConfiguration(); XCTAssertEqual(cached?.revision, 1)
        try await transport.connect()
        let before = await hub.operations.count
        do { _ = try await transport.applyDraft(initial); XCTFail("Blind retry") } catch { }
        let after = await hub.operations.count; XCTAssertEqual(before, after)
        let readback = try await transport.configuration(); XCTAssertEqual(readback.revision, 2)
        let secondPID = await hub.processIdentifier(); XCTAssertNotEqual(firstPID, secondPID)
        let next = try await transport.applyDraft(readback); XCTAssertEqual(next.revision, 3)
        await transport.disconnect()
    }
    func testCIOAfterCommitMarkerStaysUnknownUntilQualifiedStoreReadback() async throws {
        let (hub, _) = try await fixture(); let transport = transport(hub)
        try await transport.connect(); let initial = try await transport.configuration()
        let pid = await hub.processIdentifier()
        try await hub.command("fail-after-marker")
        do { _ = try await transport.applyDraft(initial); XCTFail() } catch { }
        let unknown = await transport.commitState(); XCTAssertEqual(unknown, .unknown)
        let cause = await transport.lastEndpointFailure()
        XCTAssertEqual(cause?.kind, .outcomeUnknown); XCTAssertEqual(cause?.storeState, .commitOrReadbackFailed)
        try await transport.connect(); _ = try await transport.supportedPIDs() // Consume connect control before restore-io.
        try await hub.command("restore-io")
        let restored = try await transport.configuration(); XCTAssertEqual(restored.revision, 2)
        let samePID = await hub.processIdentifier(); XCTAssertEqual(pid, samePID)
        let next = try await transport.applyDraft(restored); XCTAssertEqual(next.revision, 3)
        await transport.disconnect()
    }
    func testCAtomicFileFailureNeverSendsACKAndRetryImportsSameSourceRecords() async throws {
        let (hub, directory) = try await fixture(); let transport = transport(hub)
        try await transport.connect(); _ = try await transport.supportedPIDs()
        let file = directory.appendingPathComponent("blocked-history.json")
        let archive = LocalLogArchive(storage: AtomicLogFileStorage(fileURL: file)); try await archive.reload()
        try FileManager.default.createDirectory(at: file, withIntermediateDirectories: false)
        let before = await hub.operations.count
        do { _ = try await transport.importHistoryPage(sessionID: XCTUnwrap(session), archive: archive, limit: 2); XCTFail() } catch { }
        let operations = await hub.operations; XCTAssertEqual(Array(operations.dropFirst(before)), ["logs.read"])
        let unchanged = await archive.sessions(); XCTAssertTrue(unchanged.isEmpty)
        try FileManager.default.removeItem(at: file)
        try await archive.reload()
        let result = try await transport.importHistoryPage(sessionID: XCTUnwrap(session), archive: archive, limit: 2)
        XCTAssertTrue(result.peerAcknowledged); XCTAssertEqual(result.acknowledgement.acknowledgedThrough.value, 4)
        let reopened = LocalLogArchive(storage: AtomicLogFileStorage(fileURL: file)); try await reopened.reload()
        let loaded = await reopened.sessions(); XCTAssertEqual(loaded.first?.records.count, 2)
        await transport.disconnect()
    }
    func testCDirtyDisconnectCannotDeliverOldID1ResponseToNewClient() async throws {
        let (hub, _) = try await fixture(); let a = CHostPacketPipe(hub: hub, client: "a")
        try await a.connect(); let old = try await a.readyConnection()
        let request = try GaugeFrameCodec.encode(.init(kind: .request, requestID: 1, payload: Data("{\"op\":\"hello\"}".utf8)))
        for offset in stride(from: 0, to: request.count, by: 20) {
            try await a.write(request.subdata(in: offset..<min(request.count, offset + 20)), generation: old.generation)
        }
        let firstPID = await hub.processIdentifier()
        // Deliberately never consume the old hello response, then use a new lease starting at ID1.
        await a.disconnect(generation: old.generation)
        let b = transport(hub, client: "b"); try await b.connect()
        let configuration = try await b.configuration(); XCTAssertEqual(configuration.revision, 1)
        let nextPID = await hub.processIdentifier(); XCTAssertNotEqual(firstPID, nextPID)
        await b.disconnect()
    }
    func testCCorruptLatestSlotExposesReadOnlyDiagnosticNeverOlderCurrentOrAutomaticApply() async throws {
        let (hub, _) = try await fixture(); let transport = transport(hub)
        try await transport.connect(); let first = try await transport.configuration()
        _ = try await transport.applyDraft(first)
        let current = try await transport.configuration(); XCTAssertEqual(current.revision, 2)
        let file = hub.storeFile
        let bytes = try Data(contentsOf: file); XCTAssertEqual(bytes.count, 8192)
        let writer = try FileHandle(forWritingTo: file)
        try writer.seek(toOffset: 4096 + 32)
        try writer.write(contentsOf: Data([bytes[4096 + 32] ^ 0x80])); try writer.synchronize(); try writer.close()
        do { _ = try await transport.configuration(); XCTFail("Older fallback must not become current") }
        catch let failure as GaugeEndpointFailure {
            XCTAssertEqual(failure.kind, .configurationNotOperational); XCTAssertEqual(failure.storeState, .readOnly)
            XCTAssertTrue(failure.localizedDescription.contains("sola lettura"))
            XCTAssertFalse(failure.localizedDescription.contains("read_only"))
        }
        let stale = await transport.lastObservedConfiguration(); XCTAssertEqual(stale?.revision, 2)
        let mustRead = await transport.requiresRead(); XCTAssertTrue(mustRead)
        let before = await hub.operations.count
        do { _ = try await transport.applyDraft(current); XCTFail("Automatic apply on stale snapshot") } catch { }
        let after = await hub.operations.count; XCTAssertEqual(before, after)
        await transport.disconnect()
        // Exercise the raw RPC diagnostic on apply, beyond the transport's earlier stale-read gate.
        let pipe = CHostPacketPipe(hub: hub, client: "a"); try await pipe.connect()
        let client = GaugeRPCClient(pipe: pipe, allowTestMutations: true, persistencePolicy: .hostDurableStore); try await client.open()
        _ = try await client.request(.hello, body: JSONEncoder().encode(GaugeEndpointRequest(op: "hello")))
        var next = current; next.revision = 3
        let apply = try JSONEncoder().encode(GaugeEndpointRequest(op: "config.apply", configuration: next, expectedRevision: 2))
        do { _ = try await client.request(.apply, body: apply, expectedAppliedRevision: 3); XCTFail() }
        catch { guard case GaugeLinkError.unknownCommit = error else { return XCTFail("\(error)") } }
        let cause = await client.lastEndpointFailure; XCTAssertEqual(cause?.kind, .configurationNotOperational); XCTAssertEqual(cause?.storeState, .readOnly)
        try await pipe.connect(); try await client.open()
        _ = try await client.request(.hello, body: JSONEncoder().encode(GaugeEndpointRequest(op: "hello")))
        do { _ = try await client.request(.configuration, body: JSONEncoder().encode(GaugeEndpointRequest(op: "config.get"))); XCTFail() }
        catch let failure as GaugeEndpointFailure { XCTAssertEqual(failure.storeState, .readOnly) }
        let latched = await client.applyOutcomeUnknown; XCTAssertTrue(latched)
        let count = await hub.operations.count
        do { _ = try await client.request(.apply, body: apply, expectedAppliedRevision: 3); XCTFail() } catch { }
        let unchanged = await hub.operations.count; XCTAssertEqual(count, unchanged)
        await client.disconnect()
    }
    func testCPendingOldReadCannotBlockNewClientOrOverlapStoreWriters() async throws {
        let (hub, _) = try await fixture(); let pipe = CHostPacketPipe(hub: hub, client: "a")
        try await pipe.connect(); let lease = try await pipe.readyConnection()
        let client = GaugeRPCClient(pipe: pipe, allowTestMutations: true, requestTimeout: .seconds(5), persistencePolicy: .hostDurableStore); try await client.open()
        _ = try await client.request(.hello, body: JSONEncoder().encode(GaugeEndpointRequest(op: "hello")))
        let initial = try await client.request(.configuration, body: JSONEncoder().encode(GaugeEndpointRequest(op: "config.get")))
        var candidate = try XCTUnwrap(initial.configuration); candidate.revision = 2
        let body = try JSONEncoder().encode(GaugeEndpointRequest(op: "config.apply", configuration: candidate, expectedRevision: 1))
        try await hub.command("drop-applied")
        let flag = CHostReceivedFlag()
        let applying = Task { try await client.request(.apply, body: body, expectedAppliedRevision: 2, onReceived: { await flag.mark() }) }
        let deadline = ContinuousClock().now.advanced(by: .seconds(2))
        while !(await flag.received), ContinuousClock().now < deadline { await Task.yield() }
        while await hub.pendingReadGeneration() != lease.generation, ContinuousClock().now < deadline { await Task.yield() }
        let waiting = await hub.pendingReadGeneration(); XCTAssertEqual(waiting, lease.generation)
        await pipe.disconnect(generation: lease.generation)
        let b = transport(hub, client: "b"); try await b.connect()
        do {
            let observed = try await b.configuration(); XCTAssertTrue([1, 2].contains(observed.revision))
        } catch let failure as GaugeEndpointFailure {
            // Interrupting the host during commit may leave a read-only fallback: not a current revision.
            XCTAssertEqual(failure.kind, .configurationNotOperational); XCTAssertEqual(failure.storeState, .readOnly)
        }
        do { _ = try await applying.value; XCTFail() } catch { }
        await b.disconnect()
    }
    func testCControlFileFailureDoesNotStrandLeaseOrStartStoreWriter() async throws {
        let (hub, _) = try await fixture(); let transport = transport(hub)
        let path = hub.controlFile
        try FileManager.default.createDirectory(at: path, withIntermediateDirectories: false)
        do { try await transport.connect(); XCTFail("Control sidecar must fail") } catch { }
        let noProcess = await hub.processIdentifier(); XCTAssertNil(noProcess)
        try FileManager.default.removeItem(at: path)
        try await transport.connect()
        let current = try await transport.configuration(); XCTAssertEqual(current.revision, 1)
        await transport.disconnect()
    }
    func testCDurableRPCStaleRevisionRejectionKeepsConnectionAndSnapshot() async throws {
        let (hub, _) = try await fixture(); let pipe = CHostPacketPipe(hub: hub, client: "a")
        try await pipe.connect()
        let client = GaugeRPCClient(pipe: pipe, allowTestMutations: true, persistencePolicy: .hostDurableStore)
        try await client.open()
        _ = try await client.request(.hello, body: JSONEncoder().encode(GaugeEndpointRequest(op: "hello")))
        let initial = try await client.request(.configuration, body: JSONEncoder().encode(GaugeEndpointRequest(op: "config.get")))
        var candidate = try XCTUnwrap(initial.configuration); candidate.revision = 2
        let body = try JSONEncoder().encode(GaugeEndpointRequest(op: "config.apply", configuration: candidate, expectedRevision: 1))
        let applied = try await client.request(.apply, body: body, expectedAppliedRevision: 2); XCTAssertEqual(applied.persistence, "durable")
        do { _ = try await client.request(.apply, body: body, expectedAppliedRevision: 2); XCTFail() }
        catch { guard case GaugeLinkError.rejected("revision_conflict") = error else { return XCTFail("\(error)") } }
        let current = try await client.request(.configuration, body: JSONEncoder().encode(GaugeEndpointRequest(op: "config.get")))
        XCTAssertEqual(current.configuration?.revision, 2)
        let unknown = await client.applyOutcomeUnknown; XCTAssertFalse(unknown)
        await client.disconnect()
    }
}
#endif

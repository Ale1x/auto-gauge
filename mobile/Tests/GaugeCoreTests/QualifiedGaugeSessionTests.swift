import Foundation
import XCTest
@testable import GaugeCore

final class QualifiedGaugeSessionTests: XCTestCase {
    private func session() async throws -> (QualifiedSessionTestPipe, QualifiedSessionTestProvider, QualifiedGaugeTransport) {
        let pipe = QualifiedSessionTestPipe(); let provider = QualifiedSessionTestProvider()
        let transport = QualifiedGaugeTransport(pipe: pipe, qualificationProvider: provider)
        try await transport.connect(); _ = try await transport.supportedPIDs(); _ = try await transport.configuration()
        return (pipe, provider, transport)
    }
    func testDefaultQualificationDeniesBeforeAnyHelloOrMutation() async throws {
        let pipe = QualifiedSessionTestPipe(); let transport = QualifiedGaugeTransport(pipe: pipe)
        do { try await transport.connect(); XCTFail("Default gate closed") } catch { if case GaugeLinkError.unqualified = error {} else { XCTFail("Unexpected error: \(error)") } }
        let sent = await pipe.opcodes; XCTAssertTrue(sent.isEmpty)
    }
    func testQualifiedSessionIdentityCatalogDraftAndDeviceRevision() async throws {
        let (pipe, provider, transport) = try await session()
        let id = try await transport.deviceIdentifier(); XCTAssertEqual(id, "QUALIFIED-HOST-TEST")
        let sessions = try await transport.historySessionIDs(); XCTAssertEqual(sessions.count, 2)
        let page = try await transport.historySessions(limit: 1); XCTAssertTrue(page.hasMore); XCTAssertEqual(page.nextAfterSessionID, sessions.first)
        var draft = DemoData.configuration; draft.revision = 400; draft.brightness.manualPercent = 67
        try await transport.validateDraft(draft)
        let applied = try await transport.applyDraft(draft)
        XCTAssertEqual(applied.revision, 2); XCTAssertEqual(draft.revision, 400)
        let state = await transport.commitState(); XCTAssertEqual(state, .appliedDeviceDurable(2))
        let calls = await provider.calls; let ops = await pipe.opcodes
        XCTAssertGreaterThanOrEqual(calls, ops.count * 2)
        let begin = await pipe.begins; let end = await pipe.ends; XCTAssertEqual(begin, end)
        await transport.disconnect()
    }
    func testWireSimulationAndWrongIdentityNeverBecomeNativeHistory() async throws {
        for wrongIdentity in [false, true] {
            let pipe = QualifiedSessionTestPipe()
            if wrongIdentity { await pipe.setDeviceID("OTHER") } else { await pipe.setSimulation(true) }
            let transport = QualifiedGaugeTransport(pipe: pipe, qualificationProvider: QualifiedSessionTestProvider())
            try await transport.connect()
            do { _ = try await transport.supportedPIDs(); XCTFail("Inconsistent source") } catch {}
            let ops = await pipe.opcodes; XCTAssertEqual(ops, ["hello"])
            await transport.disconnect()
        }
    }
    func testRevokedQualificationPreventsNewRPCAndLostApplyIsUnknownUntilRead() async throws {
        let (pipe, provider, transport) = try await session()
        await provider.setAllowed(false)
        let before = await pipe.opcodes
        do { try await transport.validateDraft(DemoData.configuration); XCTFail("Revoked") } catch {}
        let after = await pipe.opcodes; XCTAssertEqual(before, after)
        await provider.setAllowed(true); await pipe.reconnect(); try await transport.connect()
        _ = try await transport.supportedPIDs(); _ = try await transport.configuration()
        await pipe.setDropApplied(true)
        do { _ = try await transport.applyDraft(DemoData.configuration); XCTFail("Lost applied") } catch {}
        let state = await transport.commitState(); XCTAssertEqual(state, .unknown)
        let needsRead = await transport.requiresRead(); XCTAssertTrue(needsRead)
        await pipe.reconnect(); try await transport.connect(); _ = try await transport.supportedPIDs()
        let actual = try await transport.configuration(); XCTAssertEqual(actual.revision, 2)
        let restored = await transport.commitState(); XCTAssertEqual(restored, .idle)
        await transport.disconnect()
    }
    func testRevocationAfterBackendMutationNeverPublishesApplied() async throws {
        let (pipe, provider, transport) = try await session(); await pipe.setRevokeOnApply(provider)
        do { _ = try await transport.applyDraft(DemoData.configuration); XCTFail("Revoked after write") } catch {}
        let state = await transport.commitState(); XCTAssertEqual(state, .unknown)
        let backend = await pipe.configuration; XCTAssertEqual(backend.revision, 2)
        await transport.disconnect()
    }
    func testImportCommitsBeforeACKAndProducesSealedReceiptOnlyForNative() async throws {
        let (pipe, _, transport) = try await session()
        let storage = QualifiedSessionTestStorage(); let archive = LocalLogArchive(storage: storage); try await archive.reload()
        await pipe.requireArchive(storage)
        let id = try LogSessionID(String(repeating: "01", count: 16))
        let imported = try await transport.importHistoryPage(sessionID: id, archive: archive, limit: 16)
        XCTAssertTrue(imported.peerAcknowledged); XCTAssertEqual(imported.qualifiedImport?.items.count, 1)
        XCTAssertEqual(imported.qualifiedImport?.deviceID, "QUALIFIED-HOST-TEST")
        let acks = await pipe.acks; XCTAssertEqual(acks, 1); XCTAssertNotNil(storage.data())
        await transport.disconnect()
    }
    func testFailedArchiveWritePreventsACKAndReceipt() async throws {
        let (pipe, _, transport) = try await session()
        let storage = QualifiedSessionTestStorage(); let archive = LocalLogArchive(storage: storage); try await archive.reload()
        storage.setFailure(true); await pipe.requireArchive(storage)
        do { _ = try await transport.importHistoryPage(sessionID: .init(String(repeating: "01", count: 16)), archive: archive, limit: 16); XCTFail("Write failure") } catch {}
        let acks = await pipe.acks; XCTAssertEqual(acks, 0); XCTAssertNil(storage.data())
        await transport.disconnect()
    }
    func testStrictSessionCatalogRejectsUnknownDuplicatesOrderingAndCursor() throws {
        let a = String(repeating: "01", count: 16), b = String(repeating: "02", count: 16)
        let good: [String: Any] = ["op": "logs.sessions", "phase": "sessions", "sessions": [a,b], "has_more": true, "next_after_session_id": b]
        let page = try GaugeHistorySessionsPage.decode(JSONSerialization.data(withJSONObject: good), after: nil, limit: 2)
        XCTAssertEqual(page.sessions.map(\.value), [a,b])
        var invalid: [[String: Any]] = []
        var value = good; value["sessions"] = [b,a]; invalid.append(value)
        value = good; value["sessions"] = [a,a]; invalid.append(value)
        value = good; value["next_after_session_id"] = a; invalid.append(value)
        value = good; value["has_more"] = false; invalid.append(value)
        value = good; value["simulation"] = false; invalid.append(value)
        value = good; value.removeValue(forKey: "next_after_session_id"); invalid.append(value)
        for entry in invalid { XCTAssertThrowsError(try GaugeHistorySessionsPage.decode(JSONSerialization.data(withJSONObject: entry), after: nil, limit: 2)) }
        XCTAssertThrowsError(try GaugeHistorySessionsPage.decode(JSONSerialization.data(withJSONObject: good), after: a, limit: 2))
        XCTAssertThrowsError(try GaugeHistorySessionsPage.decode(JSONSerialization.data(withJSONObject: good), after: nil, limit: 1))
    }
    func testQualifiedInboxPersistsBeforeAccountAndDrainsAfterOutboxCommit() async throws {
        let (pipe, _, transport) = try await session()
        let archiveStorage = QualifiedSessionTestStorage(); let archive = LocalLogArchive(storage: archiveStorage); try await archive.reload(); await pipe.requireArchive(archiveStorage)
        let imported = try await transport.importHistoryPage(sessionID: .init(String(repeating: "01", count: 16)), archive: archive, limit: 16)
        let receipt = try XCTUnwrap(imported.qualifiedImport)
        let inboxStorage = QualifiedSessionTestStorage(); let inbox = QualifiedHistoryInbox(storage: inboxStorage)
        try await inbox.enqueue(receipt); try await inbox.enqueue(receipt)
        let count = await inbox.count(); XCTAssertEqual(count, 1)
        let reopened = QualifiedHistoryInbox(storage: inboxStorage); try await reopened.reload()
        let outboxStorage = QualifiedSessionTestStorage(); let outbox = CloudHistoryOutbox(storage: outboxStorage); try await outbox.reload()
        do { try await reopened.move(to: outbox); XCTFail("No account") } catch {}
        let preserved = await reopened.count(); XCTAssertEqual(preserved, 1)
        try await outbox.selectAccount("local-test-account"); outboxStorage.setFailure(true)
        do { try await reopened.move(to: outbox); XCTFail("Outbox failure") } catch {}
        let stillPresent = await reopened.count(); XCTAssertEqual(stillPresent, 1)
        outboxStorage.setFailure(false); try await reopened.move(to: outbox)
        let empty = await reopened.count(); XCTAssertEqual(empty, 0)
        let status = await outbox.status(); XCTAssertEqual(status.pending, 1)
        await transport.disconnect()
    }
}

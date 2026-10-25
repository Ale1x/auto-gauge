import Foundation
import XCTest
@testable import GaugeCore

private final class APP15FaultStorage: LogArchiveStorage, @unchecked Sendable {
    private let lock = NSLock()
    private var bytes: Data?
    private var failure = 0
    func failNext(afterCommit: Bool) { lock.lock(); failure = afterCommit ? 2 : 1; lock.unlock() }
    func replace(_ value: Data?) { lock.lock(); bytes = value; lock.unlock() }
    func data() -> Data? { lock.lock(); defer { lock.unlock() }; return bytes }
    func read(maximumBytes: Int) throws -> Data? {
        lock.lock(); defer { lock.unlock() }
        guard (bytes?.count ?? 0) <= maximumBytes else { throw LogImportError.capacity }; return bytes
    }
    func write(_ data: Data) throws {
        lock.lock(); defer { lock.unlock() }
        let fault = failure; failure = 0
        if fault == 1 { throw LogImportError.capacity }
        bytes = data
        if fault == 2 { throw LogImportError.capacity }
    }
}

final class APP15HistoryRecoveryTests: XCTestCase {
    private func page() throws -> Data {
        let record = try HistoricalLogRecord(sequence: 2, monotonicMS: 100, pid: 4, ecuSource: 0x7e8, status: .valid, rawHex: "01")
        return try JSONEncoder().encode(HistoricalLogPage(sessionID: .init(String(repeating: "01", count: 16)), records: [record], lost: [.init(first: 1, last: 1)]))
    }
    private func importQualified(_ archive: LocalLogArchive) async throws {
        _ = try await archive.importQualifiedPage(deviceID: "QUALIFIED-TEST", data: page())
    }
    func testRestartAfterArchiveCommitBeforeInboxEnqueueRecoversRecordsAndLoss() async throws {
        let file = APP15FaultStorage(); let inboxFile = APP15FaultStorage()
        let archive = LocalLogArchive(storage: file); try await archive.reload(); try await importQualified(archive)
        let snapshot = try XCTUnwrap(file.data())
        let object = try XCTUnwrap(JSONSerialization.jsonObject(with: snapshot) as? [String: Any])
        XCTAssertEqual(object["schemaVersion"] as? Int, 2)
        XCTAssertEqual((object["qualifiedPending"] as? [Any])?.count, 2)
        // No receipt returned to AppModel is required after the process restarts.
        let reopened = LocalLogArchive(storage: file); try await reopened.reload()
        let inbox = QualifiedHistoryInbox(storage: inboxFile)
        try await reopened.recoverQualifiedImports(to: inbox)
        let count = await inbox.count(); XCTAssertEqual(count, 2)
        let pending = await reopened.pendingQualifiedCount(); XCTAssertEqual(pending, 0)
        let cursor = await reopened.cursor(deviceID: "QUALIFIED-TEST", sessionID: String(repeating: "01", count: 16)); XCTAssertEqual(cursor, 2)
        let final = LocalLogArchive(storage: file); try await final.reload(); try await final.recoverQualifiedImports(to: inbox)
        let unchanged = await inbox.count(); XCTAssertEqual(unchanged, 2)
    }
    func testInboxFailureBeforeOrAfterCommitRetainsIntentAndRetriesIdempotently() async throws {
        for afterCommit in [false, true] {
            let file = APP15FaultStorage(); let inboxFile = APP15FaultStorage()
            let archive = LocalLogArchive(storage: file); try await archive.reload(); try await importQualified(archive)
            let inbox = QualifiedHistoryInbox(storage: inboxFile); inboxFile.failNext(afterCommit: afterCommit)
            do { try await archive.recoverQualifiedImports(to: inbox); XCTFail("Injected inbox error") } catch {}
            let preserved = await archive.pendingQualifiedCount(); XCTAssertEqual(preserved, 2)
            let reopened = LocalLogArchive(storage: file); try await reopened.reload()
            let reopenedInbox = QualifiedHistoryInbox(storage: inboxFile)
            try await reopened.recoverQualifiedImports(to: reopenedInbox)
            let count = await reopenedInbox.count(); XCTAssertEqual(count, 2)
            let pending = await reopened.pendingQualifiedCount(); XCTAssertEqual(pending, 0)
        }
    }
    func testArchiveClearFailureBeforeOrAfterCommitKeepsInboxAndRequiresReload() async throws {
        for afterCommit in [false, true] {
            let file = APP15FaultStorage(); let inboxFile = APP15FaultStorage()
            let archive = LocalLogArchive(storage: file); try await archive.reload(); try await importQualified(archive)
            let inbox = QualifiedHistoryInbox(storage: inboxFile)
            file.failNext(afterCommit: afterCommit)
            do { try await archive.recoverQualifiedImports(to: inbox); XCTFail("Injected archive-clear error") } catch {}
            let ready = await archive.isReady(); XCTAssertFalse(ready)
            let count = await inbox.count(); XCTAssertEqual(count, 2)
            do { try await importQualified(archive); XCTFail("Must reconcile uncertain file") } catch {}
            try await archive.reload(); try await archive.recoverQualifiedImports(to: inbox)
            let preserved = await inbox.count(); XCTAssertEqual(preserved, 2)
            let pending = await archive.pendingQualifiedCount(); XCTAssertEqual(pending, 0)
        }
    }
    func testUncertainImportCannotOverwriteCommittedIntentAndRestartFindsIt() async throws {
        let file = APP15FaultStorage(); let archive = LocalLogArchive(storage: file); try await archive.reload()
        file.failNext(afterCommit: true)
        do { try await importQualified(archive); XCTFail("Response lost after replacement") } catch {}
        let ready = await archive.isReady(); XCTAssertFalse(ready)
        do { _ = try await archive.importPage(deviceID: "SIMULATED", data: page()); XCTFail("May overwrite intent") } catch {}
        try await archive.reload()
        let inbox = QualifiedHistoryInbox(storage: APP15FaultStorage()); try await archive.recoverQualifiedImports(to: inbox)
        let count = await inbox.count(); XCTAssertEqual(count, 2)
    }
    func testLegacyAndSimulatorImportsNeverAcquireQualifiedProvenance() async throws {
        let file = APP15FaultStorage(); let archive = LocalLogArchive(storage: file); try await archive.reload()
        _ = try await archive.importPage(deviceID: "SIMULATED", data: page())
        var object = try XCTUnwrap(JSONSerialization.jsonObject(with: XCTUnwrap(file.data())) as? [String: Any])
        object["schemaVersion"] = 1; object.removeValue(forKey: "qualifiedPending")
        file.replace(try JSONSerialization.data(withJSONObject: object))
        let legacy = LocalLogArchive(storage: file); try await legacy.reload()
        let inbox = QualifiedHistoryInbox(storage: APP15FaultStorage()); try await legacy.recoverQualifiedImports(to: inbox)
        let count = await inbox.count(); XCTAssertEqual(count, 0)
        let sessions = await legacy.sessions(); XCTAssertEqual(sessions.count, 1)
        _ = try await legacy.importPage(deviceID: "SIMULATED", data: page())
        let pending = await legacy.pendingQualifiedCount(); XCTAssertEqual(pending, 0)
    }
    func testPendingItemMustMatchCommittedArchiveAndMissingFileClearsRAMProjection() async throws {
        let file = APP15FaultStorage(); let archive = LocalLogArchive(storage: file); try await archive.reload(); try await importQualified(archive)
        var object = try XCTUnwrap(JSONSerialization.jsonObject(with: XCTUnwrap(file.data())) as? [String: Any])
        object["sessions"] = []
        file.replace(try JSONSerialization.data(withJSONObject: object))
        do { try await archive.reload(); XCTFail("Orphan qualified intent") } catch {}
        file.replace(nil); try await archive.reload()
        let sessions = await archive.sessions(); XCTAssertTrue(sessions.isEmpty)
        let pending = await archive.pendingQualifiedCount(); XCTAssertEqual(pending, 0)
    }
}

import Foundation
import XCTest
@testable import GaugeCore

private final class RestoreMemoryStorage: LogArchiveStorage, @unchecked Sendable {
    var data: Data?
    var fail = false
    func read(maximumBytes: Int) throws -> Data? { data }
    func write(_ data: Data) throws {
        if fail { throw CloudSyncError.unavailable }
        self.data = data
    }
}
private actor PullFakeDatabase: CloudHistoryPullDatabase {
    var current: CloudAccount = .available(context: "A")
    var pages: [String: CloudHistoryPullPage] = [:]
    var offlineCursor: String?
    var expiredCursor: String?
    var reads: [String] = []
    var pause = false
    var started = false
    var waiter: CheckedContinuation<Void, Never>?
    var pauseAccount = false
    var accountStarted = false
    var accountWaiter: CheckedContinuation<Void, Never>?
    func account() async throws -> CloudAccount {
        accountStarted = true
        if pauseAccount { await withCheckedContinuation { accountWaiter = $0 } }
        return current
    }
    func putImmutable(_ item: CloudHistoryItem, context: String) async throws { throw CloudSyncError.unavailable }
    func readHistory(context: String, cursor: Data?, limit: Int) async throws -> CloudHistoryPullPage {
        let key = cursor.map { String(decoding: $0, as: UTF8.self) } ?? "START"
        reads.append(key); started = true
        if pause { await withCheckedContinuation { waiter = $0 } }
        if offlineCursor == key { throw CloudSyncError.unavailable }
        if expiredCursor == key { throw CloudSyncError.cursorExpired }
        guard let page = pages[key] else { throw CloudSyncError.invalidData }
        return page
    }
    func configure(_ pages: [String: CloudHistoryPullPage]) { self.pages = pages }
    func offline(at value: String?) { offlineCursor = value }
    func expire(at value: String?) { expiredCursor = value }
    func setAccount(_ value: CloudAccount) { current = value }
    func suspendRead() { pause = true }
    func resumeRead() { pause = false; waiter?.resume(); waiter = nil }
    func suspendAccount() { pauseAccount = true }
    func resumeAccount() { pauseAccount = false; accountWaiter?.resume(); accountWaiter = nil }
}

final class CloudRestoreTests: XCTestCase {
    private func item(_ sequence: UInt64 = 1, raw: String = "01") throws -> CloudHistoryItem {
        try .init(deviceID: "TEST-GAUGE", sessionID: .init(String(repeating: "b", count: 32)), record: .init(sequence: sequence, monotonicMS: UInt64.max, pid: 12, ecuSource: 0x7e8, status: .valid, rawHex: raw))
    }
    private func page(_ items: [CloudHistoryItem], next: String? = nil) throws -> CloudHistoryPullPage {
        try .init(records: items.map { try .init(item: $0) }, nextCursor: next.map { Data($0.utf8) })
    }
    private func store(_ storage: RestoreMemoryStorage) async throws -> CloudHistoryRestoreStore {
        let store = CloudHistoryRestoreStore(storage: storage); try await store.reload(); try await store.selectAccount("A"); return store
    }
    func testLossObservationAndAvailableRecordsCoexistAcrossPhones() async throws {
        let storage = RestoreMemoryStorage(); let store = try await store(storage); let db = PullFakeDatabase()
        let loss = try CloudHistoryItem(deviceID: "TEST-GAUGE", sessionID: .init(String(repeating: "b", count: 32)), loss: .init(first: 1, last: 2))
        await db.configure(["START": try page([loss], next: "phoneB"), "phoneB": try page([item(1), item(2)])])
        let result = try await store.restore(database: db)
        XCTAssertTrue(result.scanFinished); XCTAssertEqual(result.recordCount, 2); XCTAssertEqual(result.observedLossCount, 1)
        let reopened = try await self.store(storage); let restored = await reopened.progress(); XCTAssertEqual(restored, result)
    }
    func testOfflineResumeCheckpointAndUInt64NoPrecisionLoss() async throws {
        let storage = RestoreMemoryStorage(); var store = try await store(storage); let db = PullFakeDatabase()
        await db.configure(["START": try page([item(9_007_199_254_740_993)], next: "next"), "next": try page([item(UInt64.max)])])
        await db.offline(at: "next")
        do { _ = try await store.restore(database: db); XCTFail() } catch { }
        let partial = await store.progress(); XCTAssertEqual(partial.recordCount, 1); XCTAssertTrue(partial.hasCheckpoint)
        store = try await self.store(storage); await db.offline(at: nil)
        let result = try await store.restore(database: db)
        XCTAssertEqual(result.items.compactMap { $0.record?.sequence.value }, [9_007_199_254_740_993, UInt64.max])
        let reads = await db.reads; XCTAssertEqual(reads, ["START", "next", "next"])
    }
    func testFailedWriteDoesNotAdvanceCursorAndRetryIsIdempotent() async throws {
        let storage = RestoreMemoryStorage(); let store = try await store(storage); let db = PullFakeDatabase()
        await db.configure(["START": try page([item()], next: "next"), "next": try page([item(), item(2)])])
        _ = try await store.restore(database: db, maximumPages: 1)
        let old = storage.data; storage.fail = true
        do { _ = try await store.restore(database: db); XCTFail() } catch { }
        XCTAssertEqual(storage.data, old)
        let failed = await store.progress(); XCTAssertEqual(failed.recordCount, 1); XCTAssertTrue(failed.hasCheckpoint)
        storage.fail = false
        let result = try await store.restore(database: db); XCTAssertEqual(result.recordCount, 2)
        let reads = await db.reads; XCTAssertEqual(reads, ["START", "next", "next"])
    }
    func testConflictingContentRejectsEntirePageAndCheckpoint() async throws {
        let storage = RestoreMemoryStorage(); let store = try await store(storage); let db = PullFakeDatabase()
        await db.configure(["START": try page([item()], next: "next"), "next": try page([item(2), item(raw: "02")])])
        _ = try await store.restore(database: db, maximumPages: 1); let old = storage.data
        do { _ = try await store.restore(database: db); XCTFail() } catch { guard case CloudSyncError.conflict = error else { return XCTFail("\(error)") } }
        XCTAssertEqual(storage.data, old)
        let result = await store.progress(); XCTAssertEqual(result.recordCount, 1); XCTAssertTrue(result.hasCheckpoint)
    }
    func testIdentityAndDigestMismatchRejectPage() async throws {
        for wrongDigest in [false, true] {
            let storage = RestoreMemoryStorage(); let store = try await store(storage); let db = PullFakeDatabase()
            let valid = try CloudHistoryRemoteRecord(item: item())
            let invalid = CloudHistoryRemoteRecord(recordID: wrongDigest ? valid.recordID : "wrong", payload: valid.payload, digest: wrongDigest ? String(repeating: "0", count: 64) : valid.digest)
            await db.configure(["START": try .init(records: [valid, invalid], nextCursor: nil)])
            do { _ = try await store.restore(database: db); XCTFail() } catch { }
            let result = await store.progress(); XCTAssertTrue(result.items.isEmpty); XCTAssertFalse(result.scanFinished); XCTAssertNil(storage.data)
        }
    }
    func testExpiredCursorRequiresExplicitRestartRetainsItemsAndRescans() async throws {
        let storage = RestoreMemoryStorage(); let store = try await store(storage); let db = PullFakeDatabase()
        await db.configure(["START": try page([item()], next: "expired")])
        _ = try await store.restore(database: db, maximumPages: 1); let old = storage.data
        await db.expire(at: "expired")
        do { _ = try await store.restore(database: db); XCTFail() } catch { guard case CloudSyncError.cursorExpired = error else { return XCTFail("\(error)") } }
        XCTAssertEqual(storage.data, old)
        try await store.restartScan()
        let retained = await store.progress(); XCTAssertEqual(retained.recordCount, 1); XCTAssertFalse(retained.hasCheckpoint)
        await db.configure(["START": try page([item(), item(2)])]); await db.expire(at: nil)
        let result = try await store.restore(database: db); XCTAssertEqual(result.recordCount, 2)
    }
    func testAccountChangeDuringReadCannotWriteOldPageIntoNewContext() async throws {
        let storage = RestoreMemoryStorage(); let store = try await store(storage); let db = PullFakeDatabase()
        await db.configure(["START": try page([item()])]); await db.suspendRead()
        let task = Task { try await store.restore(database: db) }
        while !(await db.started) { await Task.yield() }
        await store.invalidateAccount(); try await store.selectAccount("B"); await db.setAccount(.available(context: "B")); await db.resumeRead()
        do { _ = try await task.value; XCTFail() } catch { }
        let b = await store.progress(); XCTAssertTrue(b.items.isEmpty); XCTAssertNil(storage.data)
        let result = try await store.restore(database: db); XCTAssertEqual(result.recordCount, 1)
        try await store.selectAccount("A"); let a = await store.progress(); XCTAssertTrue(a.items.isEmpty)
    }
    func testStaleAccountCallbackCannotInvalidateNewContext() async throws {
        let storage = RestoreMemoryStorage(); let store = try await store(storage); let db = PullFakeDatabase()
        await db.configure(["START": try page([item()])]); await db.suspendAccount()
        let task = Task { try await store.restore(database: db) }
        while !(await db.accountStarted) { await Task.yield() }
        await store.invalidateAccount(); try await store.selectAccount("B"); await db.setAccount(.available(context: "B")); await db.resumeAccount()
        do { _ = try await task.value; XCTFail() } catch { }
        let result = try await store.restore(database: db); XCTAssertEqual(result.recordCount, 1)
    }
    func testCancellationAfterReadBeforeCommitPreservesSnapshot() async throws {
        let storage = RestoreMemoryStorage(); let store = try await store(storage); let db = PullFakeDatabase()
        await db.configure(["START": try page([item()])]); await db.suspendRead()
        let task = Task { try await store.restore(database: db) }
        while !(await db.started) { await Task.yield() }
        task.cancel(); await db.resumeRead()
        do { _ = try await task.value; XCTFail() } catch { }
        let result = await store.progress(); XCTAssertTrue(result.items.isEmpty); XCTAssertNil(storage.data)
    }
    func testPageActionBudgetEmptyPagesAndFinishedNotSnapshot() async throws {
        let storage = RestoreMemoryStorage(); let store = try await store(storage); let db = PullFakeDatabase()
        await db.configure(["START": try page([], next: "1"), "1": try page([item()], next: "2"), "2": try page([item(2)])])
        let partial = try await store.restore(database: db, maximumPages: 1); XCTAssertTrue(partial.hasCheckpoint); XCTAssertTrue(partial.items.isEmpty)
        let end = try await store.restore(database: db); XCTAssertTrue(end.scanFinished); XCTAssertEqual(end.recordCount, 2)
        await db.configure(["START": try page([item(), item(2), item(3)])])
        let unchanged = try await store.restore(database: db); XCTAssertEqual(unchanged.recordCount, 2)
        try await store.restartScan()
        let rescanned = try await store.restore(database: db); XCTAssertEqual(rescanned.recordCount, 3)
    }
    func testDistinctDeviceAndSessionIdentitiesDoNotConflict() async throws {
        let storage = RestoreMemoryStorage(); let store = try await store(storage); let db = PullFakeDatabase()
        let first = try item()
        let otherDevice = try CloudHistoryItem(deviceID: "OTHER-GAUGE", sessionID: first.sessionID, record: XCTUnwrap(first.record))
        let otherSession = try CloudHistoryItem(deviceID: first.deviceID, sessionID: .init(String(repeating: "c", count: 32)), record: XCTUnwrap(first.record))
        await db.configure(["START": try page([first, otherDevice, otherSession])])
        let result = try await store.restore(database: db); XCTAssertEqual(result.recordCount, 3)
    }
    func testSnapshotCountBytesAndActionBoundsPreservePreviousData() async throws {
        let storage = RestoreMemoryStorage(); let store = try await store(storage); let db = PullFakeDatabase()
        await db.configure(["START": try page([item()])])
        _ = try await store.restore(database: db)
        let old = storage.data
        let encoded = try (1...1025).map { try JSONSerialization.jsonObject(with: item(UInt64($0)).payload()) }
        storage.data = try JSONSerialization.data(withJSONObject: ["schemaVersion": 1, "accounts": ["A": ["items": encoded, "scanFinished": false]]])
        do { try await store.reload(); XCTFail() } catch { }
        let unchanged = await store.progress(); XCTAssertEqual(unchanged.recordCount, 1)
        storage.data = Data(repeating: 32, count: CloudHistoryRestoreStore.maximumBytes + 1)
        do { try await store.reload(); XCTFail() } catch { }
        storage.data = old; try await store.reload()
        let readsBefore = await db.reads
        do { _ = try await store.restore(database: db, maximumPages: 9); XCTFail() } catch { }
        let readsAfter = await db.reads; XCTAssertEqual(readsBefore, readsAfter)
    }
    func testBoundsNonprogressingCursorAndCorruptReload() async throws {
        XCTAssertThrowsError(try CloudHistoryPullPage(records: (1...17).map { try .init(item: item(UInt64($0))) }, nextCursor: nil))
        XCTAssertThrowsError(try CloudHistoryPullPage(records: [], nextCursor: Data(repeating: 1, count: 65537)))
        XCTAssertThrowsError(try CloudHistoryPullPage(records: [], nextCursor: Data()))
        let storage = RestoreMemoryStorage(); let store = try await store(storage); let db = PullFakeDatabase()
        await db.configure(["START": try page([item()], next: "same"), "same": try page([item(2)], next: "same")])
        _ = try await store.restore(database: db, maximumPages: 1); let old = storage.data
        do { _ = try await store.restore(database: db); XCTFail() } catch { }
        XCTAssertEqual(storage.data, old)
        storage.data = Data("{}".utf8)
        do { try await store.reload(); XCTFail() } catch { }
        let before = await store.progress(); XCTAssertEqual(before.recordCount, 1)
        do { _ = try await store.restore(database: db); XCTFail() } catch { }
        storage.data = old
    }
}

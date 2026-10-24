import Foundation
import XCTest
@testable import GaugeCore

private final class CloudMemoryStorage: LogArchiveStorage, @unchecked Sendable {
    var data: Data?
    var fail = false
    func read(maximumBytes: Int) throws -> Data? { data }
    func write(_ value: Data) throws {
        if fail { throw CloudSyncError.unavailable }; data = value
    }
}
private actor FakeCloudDatabase: CloudHistoryDatabase {
    var current: CloudAccount = .available(context: "A")
    var values: [String: CloudHistoryItem] = [:]
    var offline = false
    var saves = 0
    var changeAfterCommit = false
    var pause = false
    var started = false
    var pauseAccount = false
    var accountStarted = false
    var accountWaiter: CheckedContinuation<Void, Never>?
    var waiter: CheckedContinuation<Void, Never>?
    func account() async throws -> CloudAccount {
        accountStarted = true
        if pauseAccount { await withCheckedContinuation { accountWaiter = $0 } }
        if offline { throw CloudSyncError.unavailable }; return current
    }
    func suspendAccount() { pauseAccount = true }
    func resumeAccount() { pauseAccount = false; accountWaiter?.resume(); accountWaiter = nil }
    func setAccount(_ account: CloudAccount) { current = account }
    func setOffline(_ value: Bool) { offline = value }
    func setPause() { pause = true }
    func switchAfterCommit(_ value: Bool) { changeAfterCommit = value }
    func seed(_ item: CloudHistoryItem) { values["A:" + item.recordID] = item }
    func release() { pause = false; waiter?.resume(); waiter = nil }
    func putImmutable(_ item: CloudHistoryItem, context: String) async throws {
        started = true
        if pause { await withCheckedContinuation { waiter = $0 } }
        try Task.checkCancellation()
        guard current == .available(context: context) else { throw CloudSyncError.accountChanged }
        if offline { throw CloudSyncError.unavailable }
        let key = context + ":" + item.recordID
        if let old = values[key], old != item { throw CloudSyncError.conflict }
        values[key] = item; saves += 1
        if changeAfterCommit { current = .available(context: "B") }
    }
    func numberSaved() -> Int { values.count }
}

final class CloudHistoryTests: XCTestCase {
    private func item(sequence: UInt64 = 1, raw: String = "01") throws -> CloudHistoryItem {
        try .init(deviceID: "TEST-DEVICE", sessionID: .init(String(repeating: "a", count: 32)), record: .init(sequence: sequence, monotonicMS: UInt64.max, pid: 4, ecuSource: 0x7e8, status: .valid, rawHex: raw))
    }
    private func ready(_ storage: CloudMemoryStorage) async throws -> CloudHistoryOutbox {
        let box = CloudHistoryOutbox(storage: storage); try await box.reload(); try await box.selectAccount("A"); return box
    }
    func testUInt64CanonicalAndImmutableIdentity() throws {
        let value = try item(sequence: UInt64.max)
        let data = try value.payload()
        XCTAssertEqual(try CloudHistoryItem.decode(data), value)
        XCTAssertTrue(String(decoding: data, as: UTF8.self).contains("\"18446744073709551615\""))
        XCTAssertEqual(value.recordID, try item(sequence: UInt64.max, raw: "02").recordID)
        XCTAssertNotEqual(value.recordID, try item(sequence: 9_007_199_254_740_993).recordID)
        let invalid = String(decoding: data, as: UTF8.self).replacingOccurrences(of: "18446744073709551615", with: "018446744073709551615")
        XCTAssertThrowsError(try CloudHistoryItem.decode(Data(invalid.utf8)))
    }
    func testDirectDecoderRejectsInvalidEnvelope() throws {
        let payload = try item().payload()
        var object = try XCTUnwrap(JSONSerialization.jsonObject(with: payload) as? [String: Any])
        for mutation in 0..<4 {
            var candidate = object
            switch mutation {
            case 0: candidate.removeValue(forKey: "record")
            case 1: candidate["loss"] = ["first": "1", "last": "2"]
            case 2: candidate["schemaVersion"] = 2
            default: candidate["unknown"] = true
            }
            let data = try JSONSerialization.data(withJSONObject: candidate)
            XCTAssertThrowsError(try JSONDecoder().decode(CloudHistoryItem.self, from: data))
        }
        object["deviceID"] = "bad device"
        XCTAssertThrowsError(try JSONDecoder().decode(CloudHistoryItem.self, from: JSONSerialization.data(withJSONObject: object)))
    }
    func testDemoRefusedAndWriteFailurePreservesSnapshot() async throws {
        let storage = CloudMemoryStorage(); let box = try await ready(storage)
        do { try await box.enqueue([item()], provenance: .simulation); XCTFail() } catch { }
        storage.fail = true
        do { try await box.enqueue([item()], provenance: .realDevice); XCTFail() } catch { }
        let status = await box.status(); XCTAssertEqual(status.pending, 0); XCTAssertNil(storage.data)
    }
    func testOfflineResumeReopenAndIdempotency() async throws {
        let storage = CloudMemoryStorage(); var box = try await ready(storage); let db = FakeCloudDatabase()
        try await box.enqueue([item()], provenance: .realDevice)
        await db.setOffline(true)
        do { _ = try await box.synchronize(database: db); XCTFail() } catch { }
        box = try await ready(storage)
        let before = await box.status(); XCTAssertEqual(before.pending, 1)
        await db.setOffline(false)
        let status = try await box.synchronize(database: db); XCTAssertEqual(status.uploaded, 1)
        try await box.enqueue([item()], provenance: .realDevice)
        _ = try await box.synchronize(database: db)
        let saves = await db.saves; XCTAssertEqual(saves, 1)
        box = try await ready(storage); let reopened = await box.status(); XCTAssertEqual(reopened.uploaded, 1)
    }
    func testCloudConflictPreservesPendingAndRemote() async throws {
        let storage = CloudMemoryStorage(); let box = try await ready(storage); let db = FakeCloudDatabase()
        let first = try item(); try await box.enqueue([first], provenance: .realDevice)
        try await db.seed(item(raw: "02"))
        do { _ = try await box.synchronize(database: db); XCTFail() } catch { guard case CloudSyncError.conflict = error else { return XCTFail("Expected conflict: \(error)") } }
        let status = await box.status(); XCTAssertEqual(status.pending, 1)
        let count = await db.numberSaved(); XCTAssertEqual(count, 1)
        do { try await box.enqueue([item(raw: "03")], provenance: .realDevice); XCTFail() } catch { }
        let remaining = await box.status(); XCTAssertEqual(remaining.pending, 1)
    }
    func testLostLocalReceiptRetriesIdenticalRemoteWithoutOverwriting() async throws {
        let storage = CloudMemoryStorage(); let box = try await ready(storage); let db = FakeCloudDatabase()
        try await box.enqueue([item()], provenance: .realDevice); storage.fail = true
        do { _ = try await box.synchronize(database: db); XCTFail() } catch { }
        let status = await box.status(); XCTAssertEqual(status.pending, 1)
        storage.fail = false
        let result = try await box.synchronize(database: db); XCTAssertEqual(result.pending, 0)
        let count = await db.numberSaved(); XCTAssertEqual(count, 1)
    }
    func testAccountChangeDuringAwaitIsolatedAndOldQueueRetained() async throws {
        let storage = CloudMemoryStorage(); let box = try await ready(storage); let db = FakeCloudDatabase()
        try await box.enqueue([item()], provenance: .realDevice); await db.setPause()
        let task = Task { try await box.synchronize(database: db) }
        while !(await db.started) { await Task.yield() }
        await box.invalidateAccount(); await db.setAccount(.available(context: "B")); try await box.selectAccount("B")
        await db.release()
        do { _ = try await task.value; XCTFail() } catch { }
        let b = await box.status(); XCTAssertEqual(b.pending, 0)
        try await box.selectAccount("A"); let a = await box.status(); XCTAssertEqual(a.pending, 1)
        let count = await db.numberSaved(); XCTAssertEqual(count, 0)
    }
    func testStaleAccountCallbackCannotInvalidateNewlySelectedContext() async throws {
        let storage = CloudMemoryStorage(); let box = try await ready(storage); let db = FakeCloudDatabase()
        try await box.enqueue([item()], provenance: .realDevice)
        await db.suspendAccount()
        let task = Task { try await box.synchronize(database: db) }
        while !(await db.accountStarted) { await Task.yield() }
        await box.invalidateAccount(); try await box.selectAccount("B")
        try await box.enqueue([item(sequence: 2)], provenance: .realDevice)
        await db.setAccount(.available(context: "B")); await db.resumeAccount()
        do { _ = try await task.value; XCTFail() } catch { }
        let current = await box.status(); XCTAssertEqual(current.pending, 1)
        // New context remains usable; the late A callback must not clear it.
        let result = try await box.synchronize(database: db); XCTAssertEqual(result.uploaded, 1)
        try await box.selectAccount("A"); let old = await box.status(); XCTAssertEqual(old.pending, 1)
    }
    func testCancelAfterAwaitPreservesPending() async throws {
        let storage = CloudMemoryStorage(); let box = try await ready(storage); let db = FakeCloudDatabase()
        try await box.enqueue([item()], provenance: .realDevice); await db.setPause()
        let task = Task { try await box.synchronize(database: db) }
        while !(await db.started) { await Task.yield() }
        task.cancel(); await db.release()
        do { _ = try await task.value; XCTFail() } catch { }
        let status = await box.status(); XCTAssertEqual(status.pending, 1)
    }
    func testLossEnvelopeAndAccountChangeAfterRemoteCommitDoNotIssueFalseReceipt() async throws {
        let storage = CloudMemoryStorage(); let box = try await ready(storage); let db = FakeCloudDatabase()
        let loss = try CloudHistoryItem(deviceID: "TEST-DEVICE", sessionID: .init(String(repeating: "a", count: 32)), loss: .init(first: 9_007_199_254_740_993, last: UInt64.max))
        XCTAssertEqual(try CloudHistoryItem.decode(loss.payload()), loss)
        try await box.enqueue([loss], provenance: .realDevice)
        await db.switchAfterCommit(true)
        do { _ = try await box.synchronize(database: db); XCTFail() } catch { }
        try await box.selectAccount("B")
        let other = await box.status(); XCTAssertEqual(other.pending, 0); XCTAssertEqual(other.uploaded, 0)
        try await box.selectAccount("A")
        let original = await box.status(); XCTAssertEqual(original.pending, 1); XCTAssertEqual(original.uploaded, 0)
        let saved = await db.numberSaved(); XCTAssertEqual(saved, 1)
        await db.setAccount(.available(context: "A"))
        // Retry confirms the identical immutable record, rather than assuming remote rollback.
        let reopened = try await ready(storage)
        let before = await reopened.status(); XCTAssertEqual(before.pending, 1)
        await db.switchAfterCommit(false)
        let result = try await reopened.synchronize(database: db); XCTAssertEqual(result.uploaded, 1)
        let deduplicated = await db.numberSaved(); XCTAssertEqual(deduplicated, 1)
    }
    func testBoundedQueueAndCorruptReloadPreserveFile() async throws {
        let storage = CloudMemoryStorage(); let box = try await ready(storage)
        try await box.enqueue([item()], provenance: .realDevice)
        let old = storage.data
        let tooMany = try (1...1025).map { try item(sequence: UInt64($0)) }
        do { try await box.enqueue(tooMany, provenance: .realDevice); XCTFail() } catch { }
        XCTAssertEqual(storage.data, old)
        storage.data = Data("{}".utf8)
        do { try await box.reload(); XCTFail() } catch { }
        let status = await box.status(); XCTAssertEqual(status.pending, 1)
        do { try await box.enqueue([item(sequence: 2)], provenance: .realDevice); XCTFail() } catch { }
    }
}

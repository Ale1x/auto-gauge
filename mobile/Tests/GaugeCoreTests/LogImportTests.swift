import Foundation
import XCTest
@testable import GaugeCore

private final class MemoryLogStorage: LogArchiveStorage, @unchecked Sendable {
    var data: Data?
    var failsWrite = false
    var failsRead = false
    enum Failure: Error { case disk }
    func read(maximumBytes: Int) throws -> Data? {
        if failsRead { throw Failure.disk }
        guard let data else { return nil }
        guard data.count <= maximumBytes else { throw LogImportError.capacity }
        return data
    }
    func write(_ data: Data) throws {
        if failsWrite { throw Failure.disk }
        self.data = data
    }
}

final class LogImportTests: XCTestCase {
    private var scenarios: [[String: Any]] {
        get throws {
            let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
            let data = try Data(contentsOf: root.appendingPathComponent("shared/fixtures/log.scenarios.json"))
            return try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [[String: Any]])
        }
    }
    private func pageData(_ page: Any) throws -> Data { try JSONSerialization.data(withJSONObject: page, options: [.sortedKeys]) }
    private func examplePage() throws -> Data {
        let first = try XCTUnwrap(scenarios.first)
        let operations = try XCTUnwrap(first["operations"] as? [[String: Any]])
        return try pageData(XCTUnwrap(operations[0]["page"]))
    }
    func testAllSharedScenariosIncludingReopenAtEveryOperation() async throws {
        for scenario in try scenarios {
            let storage = MemoryLogStorage()
            var archive = LocalLogArchive(storage: storage)
            try await archive.reload()
            let name = try XCTUnwrap(scenario["name"] as? String)
            let device = try XCTUnwrap(scenario["device_id"] as? String)
            for operation in try XCTUnwrap(scenario["operations"] as? [[String: Any]]) {
                let rawPage = try XCTUnwrap(operation["page"] as? [String: Any])
                let data = try pageData(rawPage)
                let expected = try XCTUnwrap(operation["accepted"] as? Bool)
                let old = await archive.sessions()
                do {
                    let ack = try await archive.importPage(deviceID: device, data: data)
                    XCTAssertTrue(expected, name)
                    XCTAssertEqual(ack.acknowledgedThrough.decimal, operation["acknowledged_through"] as? String, name)
                } catch {
                    XCTAssertFalse(expected, "\(name): \(error)")
                    let after = await archive.sessions()
                    XCTAssertEqual(old, after, name)
                }
                let sessionID = try XCTUnwrap(rawPage["session_id"] as? String)
                let cursor = await archive.cursor(deviceID: device, sessionID: sessionID)
                XCTAssertEqual(String(cursor), operation["acknowledged_through"] as? String, name)
                let sessions = await archive.sessions()
                XCTAssertEqual(sessions.reduce(0, { $0 + $1.records.count }), operation["record_count"] as? Int, name)
                archive = LocalLogArchive(storage: storage)
                try await archive.reload()
                let reloaded = await archive.sessions()
                XCTAssertEqual(reloaded, sessions, name)
            }
        }
    }
    func testWriteAndReloadFailuresPreserveSnapshotAndDoNotIssueACK() async throws {
        let storage = MemoryLogStorage()
        let archive = LocalLogArchive(storage: storage)
        let data = try examplePage()
        do { _ = try await archive.importPage(deviceID: "demo", data: data); XCTFail("Import before load") } catch { }
        try await archive.reload()
        storage.failsWrite = true
        do { _ = try await archive.importPage(deviceID: "demo", data: data); XCTFail("ACK after write failure") } catch { }
        var sessions = await archive.sessions()
        XCTAssertTrue(sessions.isEmpty)
        storage.failsWrite = false
        try await archive.reload() // Failed replacement requires reconciliation before retry.
        let ack = try await archive.importPage(deviceID: "demo", data: data)
        XCTAssertEqual(ack.acknowledgedThrough.value, 2)
        sessions = await archive.sessions()
        storage.data = Data("{corrotto".utf8)
        do { try await archive.reload(); XCTFail("Corrupt load accepted") } catch { }
        let preserved = await archive.sessions()
        XCTAssertEqual(preserved, sessions)
        do { _ = try await archive.importPage(deviceID: "demo", data: data); XCTFail("Import after failed reload") } catch { }
        storage.failsRead = true
        do { try await archive.reload(); XCTFail("Read failure accepted") } catch { }
        let stillPreserved = await archive.sessions()
        XCTAssertEqual(stillPreserved, sessions)
    }
    func testCanonicalCountersStrictTypesAndUnknownDuplicateKeys() throws {
        let valid = try examplePage()
        let original = try XCTUnwrap(JSONSerialization.jsonObject(with: valid) as? [String: Any])
        for value: Any in ["00", "01", "-1", "18446744073709551616", "1.0", "１", 1, true] {
            var page = original
            var records = try XCTUnwrap(page["records"] as? [[String: Any]])
            records[0]["sequence"] = value
            page["records"] = records
            XCTAssertThrowsError(try HistoricalLogPage.decode(pageData(page)))
        }
        var page = original
        page["lost"] = NSNull()
        XCTAssertThrowsError(try HistoricalLogPage.decode(pageData(page)))
        page = original; page["extra"] = "x"
        XCTAssertThrowsError(try HistoricalLogPage.decode(pageData(page)))
        let text = try XCTUnwrap(String(data: valid, encoding: .utf8))
        let duplicate = text.replacingOccurrences(of: "\"session_id\":", with: "\"session_id\":\"01010101010101010101010101010101\",\"session_id\":")
        XCTAssertThrowsError(try HistoricalLogPage.decode(Data(duplicate.utf8)))
        let fractionalPID = text.replacingOccurrences(of: "\"pid\":12", with: "\"pid\":12.0")
        let exponentPID = text.replacingOccurrences(of: "\"pid\":12", with: "\"pid\":12e0")
        XCTAssertThrowsError(try HistoricalLogPage.decode(Data(fractionalPID.utf8)))
        XCTAssertThrowsError(try HistoricalLogPage.decode(Data(exponentPID.utf8)))
    }
    func testRecordAndSessionTypesRejectInvalidPrimitiveFields() throws {
        let original = try XCTUnwrap(JSONSerialization.jsonObject(with: examplePage()) as? [String: Any])
        let invalidFields: [(String, Any)] = [("pid", true), ("ecu_source", true), ("ecu_source", 536870912),
            ("status", "unknown"), ("raw_hex", "a"), ("raw_hex", "AB"), ("monotonic_ms", "01")]
        for (key, value) in invalidFields {
            var page = original
            var records = try XCTUnwrap(page["records"] as? [[String: Any]])
            records[0][key] = value
            page["records"] = records
            XCTAssertThrowsError(try HistoricalLogPage.decode(pageData(page)), key)
        }
        for invalid in [String(repeating: "0", count: 32), String(repeating: "A", count: 32), "123"] {
            var page = original; page["session_id"] = invalid
            XCTAssertThrowsError(try HistoricalLogPage.decode(pageData(page)))
        }
    }
    func testIndependentPhoneArchivesDoNotShareCursorOrEraseRecords() async throws {
        let first = LocalLogArchive(storage: MemoryLogStorage())
        let second = LocalLogArchive(storage: MemoryLogStorage())
        try await first.reload(); try await second.reload()
        let data = try examplePage()
        _ = try await first.importPage(deviceID: "demo", data: data)
        let before = await second.sessions()
        XCTAssertTrue(before.isEmpty)
        _ = try await second.importPage(deviceID: "demo", data: data)
        let firstSessions = await first.sessions(), secondSessions = await second.sessions()
        XCTAssertEqual(firstSessions, secondSessions)
        XCTAssertEqual(firstSessions.first?.records.count, 2)
    }
    func testLocalRetentionLimitRejectsWithoutEvictionOrACK() async throws {
        let storage = MemoryLogStorage()
        let archive = LocalLogArchive(storage: storage)
        try await archive.reload()
        let record = try HistoricalLogRecord(sequence: 1, monotonicMS: .max, pid: 12, ecuSource: 2024, status: .timeout, rawHex: "")
        XCTAssertEqual(record.relativeTimeIT, "+18446744073709551 s · 615 ms dall’avvio")
        for i in 1...LocalLogArchive.maximumSessions {
            let session = try LogSessionID(String(format: "%032x", i))
            let page = try HistoricalLogPage(sessionID: session, records: [record])
            _ = try await archive.importPage(deviceID: "demo", data: JSONEncoder().encode(page))
        }
        let before = await archive.sessions(), bytes = storage.data
        let overflow = try HistoricalLogPage(sessionID: LogSessionID(String(repeating: "f", count: 32)), records: [record])
        do { _ = try await archive.importPage(deviceID: "demo", data: JSONEncoder().encode(overflow)); XCTFail("Retention exceeded") } catch { }
        let after = await archive.sessions()
        XCTAssertEqual(after, before)
        XCTAssertEqual(storage.data, bytes)
    }
    func testBundledDemoPagesUseRealImporterAndRemainIdempotent() async throws {
        let demo = try DemoLogImport.load()
        let storage = MemoryLogStorage()
        let archive = LocalLogArchive(storage: storage)
        try await archive.reload()
        for page in demo.pages { _ = try await archive.importPage(deviceID: demo.deviceID, data: page) }
        let first = await archive.sessions()
        XCTAssertEqual(first.count, 3)
        XCTAssertTrue(first.contains(where: { !$0.lost.isEmpty }))
        XCTAssertTrue(first.contains(where: { $0.records.contains(where: { $0.status == .timeout }) }))
        for page in demo.pages { _ = try await archive.importPage(deviceID: demo.deviceID, data: page) }
        let replay = await archive.sessions()
        XCTAssertEqual(first, replay)
    }
    func testRecordRetentionLimitRefusesNewPageWithoutEviction() async throws {
        let storage = MemoryLogStorage()
        let archive = LocalLogArchive(storage: storage)
        try await archive.reload()
        let session = try LogSessionID(String(repeating: "04", count: 16))
        for start in stride(from: 1, through: LocalLogArchive.maximumRecords, by: 16) {
            let records = try (start..<(start+16)).map {
                try HistoricalLogRecord(sequence: UInt64($0), monotonicMS: UInt64($0*1000), pid: 12, ecuSource: 2024, status: .valid, rawHex: "1a10")
            }
            let page = try HistoricalLogPage(sessionID: session, records: records)
            _ = try await archive.importPage(deviceID: "demo", data: JSONEncoder().encode(page))
        }
        let before = await archive.sessions(), bytes = storage.data
        let record = try HistoricalLogRecord(sequence: 1025, monotonicMS: 1025000, pid: 12, ecuSource: 2024, status: .valid, rawHex: "1a10")
        let page = try HistoricalLogPage(sessionID: session, records: [record])
        do { _ = try await archive.importPage(deviceID: "demo", data: JSONEncoder().encode(page)); XCTFail("Record retention exceeded") } catch { }
        let after = await archive.sessions()
        XCTAssertEqual(after, before)
        XCTAssertEqual(storage.data, bytes)
    }
    func testAtomicFileStorageResumesAndRejectsOversizedFileWithoutReplacingSnapshot() async throws {
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: directory) }
        let url = directory.appendingPathComponent("logs.json")
        let storage = AtomicLogFileStorage(fileURL: url)
        let archive = LocalLogArchive(storage: storage)
        try await archive.reload()
        _ = try await archive.importPage(deviceID: "demo", data: examplePage())
        let before = await archive.sessions()
        let reopened = LocalLogArchive(storage: storage)
        try await reopened.reload()
        let after = await reopened.sessions()
        XCTAssertEqual(after, before)
        try Data(repeating: 32, count: LocalLogArchive.maximumBytes+1).write(to: url)
        do { try await archive.reload(); XCTFail("Oversized file accepted") } catch { }
        let preserved = await archive.sessions()
        XCTAssertEqual(preserved, before)
    }
}

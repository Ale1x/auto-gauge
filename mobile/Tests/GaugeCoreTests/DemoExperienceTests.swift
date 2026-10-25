import Foundation
import XCTest
@testable import GaugeCore

final class DemoExperienceTests: XCTestCase {
    private func directory() throws -> URL {
        let url = FileManager.default.temporaryDirectory.appending(path: "ag-demo-experience-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        addTeardownBlock { try? FileManager.default.removeItem(at: url) }
        return url
    }
    private func richArchive(_ directory: URL) async throws -> LocalLogArchive {
        let archive = LocalLogArchive(storage: AtomicLogFileStorage(fileURL: directory.appending(path: "histories.json")))
        try await archive.reload()
        for trip in try DemoJourneyFixture.trips {
            for page in try DemoJourneyFixture.pages(for: trip) {
                _ = try await archive.importPage(deviceID: DemoJourneyFixture.deviceID, data: page)
            }
        }
        return archive
    }
    func testTwoRichTripsRespectBoundsAndRepeatedRefreshDoesNotDuplicate() async throws {
        let directory = try directory(); let archive = try await richArchive(directory)
        let first = await archive.sessions()
        XCTAssertEqual(first.count, 2); XCTAssertEqual(first.reduce(0) { $0 + $1.records.count }, 960)
        for session in first {
            XCTAssertEqual(Set(session.records.map(\.pid)), [4,5,12,13])
            let timeline = try HistoryTimelineSession.imported([session])
            let projection = try HistoryTimelineProjector.project(XCTUnwrap(timeline.first), pid: 13, ecu: 0x7e8)
            XCTAssertFalse(projection.rows.isEmpty)
        }
        for trip in try DemoJourneyFixture.trips {
            for page in try DemoJourneyFixture.pages(for: trip) { _ = try await archive.importPage(deviceID: DemoJourneyFixture.deviceID, data: page) }
        }
        let repeated = await archive.sessions(); XCTAssertEqual(first, repeated)
        let pending = await archive.pendingQualifiedCount(); XCTAssertEqual(pending, 0)
    }
    func testBackupAndRestoreAreIsolatedLocalFilesAndNeverCreateQualifiedIntent() async throws {
        let root = try directory(); let demo = root.appending(path: "demo")
        let native = root.appending(path: "native-histories.json")
        let marker = Data("preserve real histories".utf8); try marker.write(to: native)
        let archive = try await richArchive(demo)
        let sessions = await archive.sessions(); let store = DemoExperienceStore(directory: demo)
        let date = Date(timeIntervalSince1970: 1_792_000_000)
        let saved = try await store.synchronize(sessions, at: date)
        XCTAssertEqual(saved.lastBackupDate, date)
        let reopened = DemoExperienceStore(directory: demo)
        let snapshot = try await reopened.backupSnapshot(); XCTAssertEqual(snapshot.sessions, sessions)
        let restored = LocalLogArchive(storage: AtomicLogFileStorage(fileURL: demo.appending(path: "restored.json"))); try await restored.reload()
        _ = try await reopened.restore(into: restored)
        let result = await restored.sessions(); XCTAssertEqual(result, sessions)
        let pending = await restored.pendingQualifiedCount(); XCTAssertEqual(pending, 0)
        XCTAssertEqual(try Data(contentsOf: native), marker)
    }
    func testBackupRejectsNativeIdentityAndKeepsPriorCopy() async throws {
        let directory = try directory(); let archive = try await richArchive(directory)
        let sessions = await archive.sessions(); let store = DemoExperienceStore(directory: directory)
        _ = try await store.synchronize(sessions)
        let before = try Data(contentsOf: directory.appending(path: "backup-v1.json"))
        var native = try XCTUnwrap(JSONSerialization.jsonObject(with: JSONEncoder().encode(sessions)) as? [[String: Any]])
        native[0]["device_id"] = "REAL-DEVICE"
        let altered = try JSONDecoder().decode([ImportedLogSession].self, from: JSONSerialization.data(withJSONObject: native))
        do { _ = try await store.synchronize(altered); XCTFail("Native histories cannot enter demo backup") } catch {}
        XCTAssertEqual(try Data(contentsOf: directory.appending(path: "backup-v1.json")), before)
    }
    func testDemoFirmwarePersistsOnlyKnownVersionAndCancellationCannotStartWrite() async throws {
        let directory = try directory(); let store = DemoExperienceStore(directory: directory)
        let first = try await store.firmwareVersion(); XCTAssertEqual(first, "0.13.0")
        let cancelled = Task {
            withUnsafeCurrentTask { $0?.cancel() }
            try await store.installFirmwareVersion("0.14.0")
        }
        do { try await cancelled.value; XCTFail() } catch {}
        let unchanged = try await store.firmwareVersion(); XCTAssertEqual(unchanged, "0.13.0")
        try await store.installFirmwareVersion("0.14.0")
        let reopened = DemoExperienceStore(directory: directory)
        let installed = try await reopened.firmwareVersion(); XCTAssertEqual(installed, "0.14.0")
        do { try await reopened.installFirmwareVersion("999"); XCTFail() } catch {}
        let preserved = try await reopened.firmwareVersion(); XCTAssertEqual(preserved, "0.14.0")
    }
    func testExplicitSaveInitializesRestartedDemoEndpointFromSavedConfiguration() async throws {
        let directory = try directory(); let storage = AtomicFileConfigurationStorage(fileURL: directory.appending(path: "configuration.json"))
        var editor = try ConfigurationDraft(configuration: DemoData.configuration, catalog: InMemoryGaugeEndpoint.catalog)
        editor.working.brightness.manualPercent = 31
        try editor.save(to: storage, catalog: InMemoryGaugeEndpoint.catalog)
        let persisted = try JSONDecoder().decode(GaugeConfiguration.self, from: XCTUnwrap(storage.read()))
        let endpoint = try InMemoryGaugeEndpoint(configuration: persisted)
        let pipe = InMemoryGaugePacketPipe(channel: endpoint, clientID: "TEST-DEMO")
        let transport = DevelopmentGaugeTransport(pipe: pipe)
        try await transport.connect(); _ = try await transport.supportedPIDs()
        let observed = try await transport.configuration()
        XCTAssertEqual(observed.brightness.manualPercent, 31); XCTAssertEqual(observed.revision, editor.saved.revision)
        await transport.disconnect()
    }
}

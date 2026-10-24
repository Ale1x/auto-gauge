import Foundation
import XCTest
@testable import GaugeCore

private actor DevelopmentPhases {
    var values: [GaugeCommitState] = []
    func append(_ phase: GaugeCommitState) { values.append(phase) }
}
final class DevelopmentTransportTests: XCTestCase {
    func testEditorAndAtomicHistoryUseUnifiedStreamingClientInMemory() async throws {
        let fixture = try DemoGaugeFixture.load()
        let endpoint = try InMemoryGaugeEndpoint(configuration: fixture.configuration, historySource: fixture.historySource)
        let transport = DevelopmentGaugeTransport(pipe: InMemoryGaugePacketPipe(channel: endpoint, clientID: "A"))
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: directory) }
        try await transport.connect()
        let catalog = try await transport.supportedPIDs()
        let initial = try await transport.configuration()
        var phoneConfig = initial; phoneConfig.revision = 42
        var editor = try ConfigurationDraft(configuration: phoneConfig, catalog: catalog)
        editor.working.brightness.manualPercent = 73
        let phoneStore = AtomicFileConfigurationStorage(fileURL: directory.appendingPathComponent("phone.json"))
        try editor.save(to: phoneStore, catalog: catalog)
        let phases = DevelopmentPhases()
        let result = try await transport.applyDraft(editor.working, onPhase: { await phases.append($0) })
        XCTAssertEqual(result.revision, 2); XCTAssertEqual(result.brightness.manualPercent, 73)
        XCTAssertEqual(editor.saved.revision, 43)
        let observedPhases = await phases.values; XCTAssertEqual(observedPhases, [.received, .appliedVolatile(2)])
        let config = try await transport.configuration(); XCTAssertEqual(config.revision, 2)
        let logFile = directory.appendingPathComponent("history.json")
        let archive = LocalLogArchive(storage: AtomicLogFileStorage(fileURL: logFile)); try await archive.reload()
        let imported = try await transport.importHistoryPage(sessionID: fixture.historySource.sessionID, archive: archive, limit: 2)
        XCTAssertTrue(imported.peerAcknowledged); XCTAssertEqual(imported.acknowledgement.acknowledgedThrough.value, 4)
        let reopened = LocalLogArchive(storage: AtomicLogFileStorage(fileURL: logFile)); try await reopened.reload()
        let sessions = await reopened.sessions(); XCTAssertEqual(sessions.first?.records.count, 2)
        await transport.disconnect()
        try await transport.connect()
        let required = await transport.requiresRead(); XCTAssertTrue(required)
        do { _ = try await transport.applyDraft(editor.working); XCTFail("Read required after reconnect") } catch { }
        let restored = try await transport.configuration(); XCTAssertEqual(restored.revision, 2)
        await transport.disconnect()
    }
    func testDurablePolicyIsExplicitAndCannotBeClaimedByVolatileMemoryBackend() async throws {
        let endpoint = try InMemoryGaugeEndpoint()
        let transport = DevelopmentGaugeTransport(pipe: InMemoryGaugePacketPipe(channel: endpoint, clientID: "HOST-C-ONLY-POLICY"), persistencePolicy: .hostDurableStore)
        try await transport.connect()
        do { _ = try await transport.configuration(); XCTFail("Volatile response must not satisfy durable policy") } catch { }
        let required = await transport.requiresRead(); XCTAssertTrue(required)
    }
}

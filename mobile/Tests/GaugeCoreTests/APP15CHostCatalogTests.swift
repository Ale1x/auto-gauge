#if os(macOS)
import Foundation
import XCTest
@testable import GaugeCore

/// Real C codec/endpoint subprocess, using explicitly trusted host fixtures only.
final class APP15CHostCatalogTests: XCTestCase {
    func testCatalogRoundTripUsesRetainedSessionsAndExclusiveCursorAcrossChanges() async throws {
        let compiled = try CHostCompiler.result.get()
        let directory = FileManager.default.temporaryDirectory.appending(path: "ag-app15-catalog-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        let hub = CHostEndpointHub(executable: compiled.binary, directory: directory)
        addTeardownBlock { await hub.shutdown(); try? FileManager.default.removeItem(at: directory) }
        let pipe = CHostPacketPipe(hub: hub, client: "a"); try await pipe.connect()
        let client = GaugeRPCClient(pipe: pipe, allowTestMutations: true, persistencePolicy: .hostDurableStore)
        try await client.open()
        _ = try await client.request(.hello, body: JSONEncoder().encode(GaugeEndpointRequest(op: "hello")))
        let initial = try await catalog(client, after: nil, limit: 1)
        let first = String(repeating: "05", count: 16)
        XCTAssertEqual(initial.sessions?.map(\.value), [first]); XCTAssertEqual(initial.hasMore, false); XCTAssertNil(initial.nextAfterSessionID)
        let empty = try await catalog(client, after: first, limit: 1)
        XCTAssertEqual(empty.sessions, []); XCTAssertEqual(empty.hasMore, false)
        try await hub.command("new-log-session")
        let next = try await catalog(client, after: first, limit: 16)
        XCTAssertEqual(next.sessions?.map(\.value), [String(repeating: "06", count: 16)])
        let refreshed = try await catalog(client, after: nil, limit: 16)
        XCTAssertEqual(refreshed.sessions, next.sessions) // Previous session is no longer retained.
        let operations = await hub.operations
        XCTAssertEqual(operations.filter { $0 == "logs.sessions" }.count, 4)
        await client.disconnect()
    }
    private func catalog(_ client: GaugeRPCClient, after: String?, limit: Int) async throws -> GaugeRPCResponse {
        try await client.request(.sessions, body: JSONEncoder().encode(GaugeEndpointRequest(op: "logs.sessions", afterSessionID: after, limit: limit)))
    }
}
#endif

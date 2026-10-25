import XCTest
@testable import GaugeCore

private actor SwitchingQualificationProvider: GaugeNativeQualificationProvider {
    let pipe: QualifiedSessionTestPipe
    let reject: Bool
    init(pipe: QualifiedSessionTestPipe, reject: Bool) { self.pipe = pipe; self.reject = reject }
    func qualification(for connection: GaugePipeConnection) async throws -> GaugeNativeQualification {
        await pipe.reconnect()
        if reject { throw GaugeLinkError.unqualified }
        return try .init(generation: connection.generation, deviceID: "QUALIFIED-HOST-TEST")
    }
}
final class QualifiedSessionLeaseReviewTests: XCTestCase {
    func testLateQualificationSuccessCannotBindOrDisconnectNewPeer() async throws { try await check(reject: false) }
    func testLateQualificationFailureCannotDisconnectNewPeer() async throws { try await check(reject: true) }
    private func check(reject: Bool) async throws {
        let pipe = QualifiedSessionTestPipe()
        let transport = QualifiedGaugeTransport(pipe: pipe, qualificationProvider: SwitchingQualificationProvider(pipe: pipe, reject: reject))
        do { try await transport.connect(); XCTFail("Generation changed during qualification") } catch {}
        let current = try await pipe.readyConnection(); XCTAssertEqual(current.generation, 72)
        let disconnects = await pipe.disconnects; XCTAssertEqual(disconnects, 0)
        let requests = await pipe.opcodes; XCTAssertTrue(requests.isEmpty)
        await pipe.disconnect(generation: current.generation)
    }
}

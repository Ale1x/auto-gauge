#if os(macOS)
import Foundation
import XCTest
@testable import GaugeCore

final class OTATransferSenderTests: XCTestCase {
    private func candidate() throws -> VerifiedOTACandidate {
        let bundle = try OTAVectorBundle.generated.get()
        let vector = try XCTUnwrap(bundle.cases.first { $0.name == "valid" })
        return try OTAPackageVerifier.verify(envelope: XCTUnwrap(Data(base64Encoded: vector.envelope_base64)),
            image: XCTUnwrap(Data(base64Encoded: bundle.image_base64)),
            trustedKeys: [bundle.key_id: XCTUnwrap(Data(base64Encoded: bundle.public_key_der_base64))],
            hardwareID: bundle.hardware_id, inactiveCapacity: bundle.inactive_capacity,
            acceptedCounter: XCTUnwrap(UInt64(bundle.accepted_counter)))
    }
    private func expectError(_ expected: OTATransferError? = nil, _ operation: () async throws -> Void) async {
        do { try await operation(); XCTFail("Expected failure") }
        catch { if let expected { XCTAssertEqual(error as? OTATransferError, expected) } }
    }
    func testDefaultGateAndInvalidInputsSendNothing() async throws {
        let candidate = try candidate(); let transport = OTATransferMockTransport()
        let sender = OTATransferSender(transport: transport)
        await expectError(.unqualified) { try await sender.upload(candidate, transferID: 1) }
        let enabled = OTATransferSender(transport: transport, allowHostDevelopmentMutations: true)
        await expectError(.invalidPayload) { try await enabled.upload(candidate, transferID: 0) }
        let invalid = OTATransferSender(transport: transport, allowHostDevelopmentMutations: true, operationTimeout: .zero)
        await expectError(.invalidTimeout) { try await invalid.upload(candidate, transferID: 1) }
        let requests = await transport.requests; XCTAssertTrue(requests.isEmpty)
    }
    func testRealVerifiedSnapshotsUploadStopsAtReadyThenExplicitSelection() async throws {
        let candidate = try candidate(); let transport = OTATransferMockTransport()
        let sender = OTATransferSender(transport: transport, allowHostDevelopmentMutations: true)
        try await sender.upload(candidate, transferID: UInt32.max)
        let state = await sender.state; let requests = await transport.requests
        let manifest = await transport.manifest; let image = await transport.image
        let flag = await sender.selectionMayHaveChanged
        XCTAssertEqual(state, .ready); XCTAssertFalse(flag)
        XCTAssertEqual(manifest, candidate.envelopeSnapshot); XCTAssertEqual(image, candidate.imageSnapshot)
        XCTAssertFalse(requests.contains { $0.opcode == .select || $0.opcode == .cancel })
        XCTAssertEqual(requests.first?.opcode, .start); XCTAssertEqual(requests.last?.opcode, .finish)
        for opcode in [OTATransferOpcode.manifest, .image] {
            var offset: UInt32 = 0
            for request in requests where request.opcode == opcode {
                XCTAssertEqual(request.offset, offset); XCTAssertTrue((1...1024).contains(request.body.count))
                offset += UInt32(request.body.count)
            }
        }
        await expectError(.notReady) { try await sender.upload(candidate, transferID: 2) }
        try await sender.select()
        let selected = await sender.state; let changed = await sender.selectionMayHaveChanged
        let selectedRequests = await transport.requests
        XCTAssertEqual(selected, .selected); XCTAssertTrue(changed)
        XCTAssertEqual(selectedRequests.filter { $0.opcode == .select }.count, 1)
        await expectError(.notReady) { try await sender.select() }
    }
    func testMalformedRejectedLostAndStaleACKsLatchUnknownWithoutRetries() async throws {
        let candidate = try candidate()
        for fault in OTATransferMockTransport.Fault.allCases where fault != .delay {
            let transport = OTATransferMockTransport(fault: fault)
            let sender = OTATransferSender(transport: transport, allowHostDevelopmentMutations: true)
            await expectError { try await sender.upload(candidate, transferID: 7) }
            let state = await sender.state; let before = await transport.requests
            XCTAssertEqual(state, .unknown, "\(fault)")
            XCTAssertEqual(before.filter { $0.opcode == .image }.count, 1, "\(fault)")
            XCTAssertFalse(before.contains { $0.opcode == .select || $0.opcode == .cancel || $0.opcode == .finish })
            await expectError(.outcomeUnknown) { try await sender.upload(candidate, transferID: 8) }
            await expectError(.outcomeUnknown) { try await sender.select() }
            let after = await transport.requests; XCTAssertEqual(after, before)
        }
    }
    func testGenerationMustBeReadyBeforeAnyDelivery() async throws {
        let transport = OTATransferMockTransport(generation: 0)
        let sender = OTATransferSender(transport: transport, allowHostDevelopmentMutations: true)
        await expectError(.notReady) { try await sender.upload(self.candidate(), transferID: 1) }
        let requests = await transport.requests; let state = await sender.state
        XCTAssertTrue(requests.isEmpty); XCTAssertEqual(state, .idle)
    }
    func testTimeoutAfterDeliveryIsTerminalAndNeverResends() async throws {
        let candidate = try candidate(); let transport = OTATransferMockTransport(fault: .delay)
        let sender = OTATransferSender(transport: transport, allowHostDevelopmentMutations: true, operationTimeout: .milliseconds(40))
        await expectError(.timeout) { try await sender.upload(candidate, transferID: 2) }
        let state = await sender.state; let requests = await transport.requests
        XCTAssertEqual(state, .unknown); XCTAssertEqual(requests.filter { $0.opcode == .image }.count, 1)
    }
    func testActorBusyAndCancellationPreserveUnknownWithoutAutomaticCancel() async throws {
        let candidate = try candidate(); let transport = OTATransferMockTransport(fault: .delay, at: .start)
        let sender = OTATransferSender(transport: transport, allowHostDevelopmentMutations: true)
        let upload = Task { try await sender.upload(candidate, transferID: 3) }
        for _ in 0..<200 {
            if !(await transport.requests).isEmpty { break }
            try await Task.sleep(for: .milliseconds(5))
        }
        let sent = await transport.requests; XCTAssertEqual(sent.count, 1)
        await expectError(.busy) { try await sender.upload(candidate, transferID: 4) }
        await expectError(.busy) { try await sender.select() }
        upload.cancel()
        do { try await upload.value; XCTFail("Expected cancellation") } catch { XCTAssertTrue(error is CancellationError) }
        let state = await sender.state; let requests = await transport.requests
        XCTAssertEqual(state, .unknown); XCTAssertEqual(requests.count, 1)
    }
    func testMalformedSelectedACKsRemainTerminalAndCannotSelectTwice() async throws {
        let candidate = try candidate()
        for fault in [OTATransferMockTransport.Fault.wrongID, .wrongPhase, .wrongManifestOffset,
                      .wrongImageOffset, .selectionFlag, .rejected, .generationChange, .trailing] {
            let transport = OTATransferMockTransport(fault: fault, at: .select)
            let sender = OTATransferSender(transport: transport, allowHostDevelopmentMutations: true)
            try await sender.upload(candidate, transferID: 11)
            await expectError { try await sender.select() }
            let state = await sender.state; let changed = await sender.selectionMayHaveChanged
            XCTAssertEqual(state, .unknown, "\(fault)"); XCTAssertTrue(changed, "\(fault)")
            await expectError(.outcomeUnknown) { try await sender.select() }
            let requests = await transport.requests
            XCTAssertEqual(requests.filter { $0.opcode == .select }.count, 1)
        }
    }
    func testCancellationBeforeDeliveryDoesNotConsumeIdleSender() async throws {
        let candidate = try candidate(); let transport = OTATransferMockTransport()
        let sender = OTATransferSender(transport: transport, allowHostDevelopmentMutations: true)
        let task = Task {
            withUnsafeCurrentTask { $0?.cancel() }
            try await sender.upload(candidate, transferID: 12)
        }
        do { try await task.value; XCTFail("Expected cancellation") } catch { XCTAssertTrue(error is CancellationError) }
        let state = await sender.state; let requests = await transport.requests
        XCTAssertEqual(state, .idle); XCTAssertTrue(requests.isEmpty)
        try await sender.upload(candidate, transferID: 13)
        let ready = await sender.state; XCTAssertEqual(ready, .ready)
    }
    func testLostSelectionAcknowledgmentLatchesPossibleSelection() async throws {
        let candidate = try candidate(); let transport = OTATransferMockTransport(fault: .lostACK, at: .select)
        let sender = OTATransferSender(transport: transport, allowHostDevelopmentMutations: true)
        try await sender.upload(candidate, transferID: 5)
        await expectError { try await sender.select() }
        let state = await sender.state; let changed = await sender.selectionMayHaveChanged
        XCTAssertEqual(state, .unknown); XCTAssertTrue(changed)
        await expectError(.outcomeUnknown) { try await sender.select() }
        let requests = await transport.requests; XCTAssertEqual(requests.filter { $0.opcode == .select }.count, 1)
    }
}
#endif

#if os(macOS)
import Foundation
import XCTest
@testable import GaugeCore

private actor APP16Session: OTAInstallationSession {
    let backend: OTATransferMockTransport
    var closed = false
    var closes = 0
    init(_ backend: OTATransferMockTransport) { self.backend = backend }
    func currentGeneration() async throws -> UInt64 {
        guard !closed else { throw OTATransferError.notReady }; return try await backend.currentGeneration()
    }
    func exchange(_ payload: Data, generation: UInt64, deadline: ContinuousClock.Instant) async throws -> Data {
        guard !closed else { throw OTATransferError.notReady }
        return try await backend.exchange(payload, generation: generation, deadline: deadline)
    }
    func close() { closed = true; closes += 1 }
}
private actor APP16Provider: OTAInstallationSessionProvider {
    let session: APP16Session
    var delay = false
    var opens = 0
    var reject = false
    init(_ session: APP16Session, delay: Bool = false, reject: Bool = false) { self.session = session; self.delay = delay; self.reject = reject }
    func openSession(for candidate: VerifiedOTACandidate, deadline: ContinuousClock.Instant) async throws -> any OTAInstallationSession {
        opens += 1
        if delay { try await Task.sleep(for: .milliseconds(100)) }
        if reject { throw OTATransferError.unqualified }
        return session
    }
}
private actor APP16Progress {
    var values: [OTAInstallationProgress] = []
    func add(_ value: OTAInstallationProgress) { values.append(value) }
}

final class APP16InstallationControllerTests: XCTestCase {
    private func candidate() throws -> VerifiedOTACandidate {
        let bundle = try OTAVectorBundle.generated.get()
        let vector = try XCTUnwrap(bundle.cases.first { $0.name == "valid" })
        return try OTAPackageVerifier.verify(envelope: XCTUnwrap(Data(base64Encoded: vector.envelope_base64)), image: XCTUnwrap(Data(base64Encoded: bundle.image_base64)), trustedKeys: [bundle.key_id: XCTUnwrap(Data(base64Encoded: bundle.public_key_der_base64))], hardwareID: bundle.hardware_id, inactiveCapacity: bundle.inactive_capacity, acceptedCounter: XCTUnwrap(UInt64(bundle.accepted_counter)))
    }
    func testNoProviderNeverStartsOrWritesAndPreflightRemainsIdle() async throws {
        let controller = OTAInstallationController()
        do { try await controller.upload(candidate(), transferID: 1); XCTFail() }
        catch { XCTAssertEqual(error as? OTATransferError, .unqualified) }
        let snapshot = await controller.snapshot(); XCTAssertEqual(snapshot.state, .idle)
        XCTAssertFalse(controller.configured)
    }
    func testUploadProgressComesFromACKsAndReadyRequiresSeparateSelect() async throws {
        let candidate = try candidate(); let backend = OTATransferMockTransport()
        let session = APP16Session(backend); let provider = APP16Provider(session)
        let controller = OTAInstallationController(provider: provider); let progress = APP16Progress()
        try await controller.upload(candidate, transferID: 55) { await progress.add($0) }
        let ready = await controller.snapshot(); XCTAssertEqual(ready.state, .ready); XCTAssertEqual(ready.manifest, candidate.manifest)
        XCTAssertEqual(ready.progress.imageAcknowledged, UInt32(candidate.imageSnapshot.count)); XCTAssertFalse(ready.selectionMayHaveChanged)
        let requests = await backend.requests; XCTAssertFalse(requests.contains { $0.opcode == .select })
        let values = await progress.values; XCTAssertGreaterThan(values.count, 2)
        XCTAssertEqual(values.last, ready.progress)
        XCTAssertTrue(zip(values, values.dropFirst()).allSatisfy { $0.imageAcknowledged <= $1.imageAcknowledged && $0.manifestAcknowledged <= $1.manifestAcknowledged })
        do { try await controller.upload(candidate, transferID: 56); XCTFail() } catch {}
        try await controller.select()
        let selected = await controller.snapshot(); XCTAssertEqual(selected.state, .selected); XCTAssertTrue(selected.selectionMayHaveChanged)
        let final = await backend.requests; XCTAssertEqual(final.filter { $0.opcode == .select }.count, 1)
        let closes = await session.closes; XCTAssertEqual(closes, 1)
        do { try await controller.select(); XCTFail() } catch {}
    }
    func testLostUploadACKAndGenerationChangeAreTerminalWithoutRetryOrSelect() async throws {
        for fault in [OTATransferMockTransport.Fault.lostACK, .generationChange] {
            let backend = OTATransferMockTransport(fault: fault)
            let session = APP16Session(backend); let provider = APP16Provider(session)
            let controller = OTAInstallationController(provider: provider)
            do { try await controller.upload(candidate(), transferID: 1); XCTFail() } catch {}
            let snapshot = await controller.snapshot(); XCTAssertEqual(snapshot.state, .unknown); XCTAssertFalse(snapshot.selectionMayHaveChanged)
            let before = await backend.requests
            do { try await controller.upload(candidate(), transferID: 2); XCTFail() } catch { XCTAssertEqual(error as? OTATransferError, .outcomeUnknown) }
            do { try await controller.select(); XCTFail() } catch { XCTAssertEqual(error as? OTATransferError, .outcomeUnknown) }
            let after = await backend.requests; XCTAssertEqual(before, after)
            let opens = await provider.opens; XCTAssertEqual(opens, 1)
            let closed = await session.closed; XCTAssertTrue(closed)
        }
    }
    func testLostSelectionACKNeverReselectsAndConservativelyMarksPossibleChange() async throws {
        let backend = OTATransferMockTransport(fault: .lostACK, at: .select)
        let controller = OTAInstallationController(provider: APP16Provider(APP16Session(backend)))
        try await controller.upload(candidate(), transferID: 1)
        do { try await controller.select(); XCTFail() } catch {}
        let snapshot = await controller.snapshot(); XCTAssertEqual(snapshot.state, .unknown); XCTAssertTrue(snapshot.selectionMayHaveChanged)
        do { try await controller.select(); XCTFail() } catch {}
        let requests = await backend.requests; XCTAssertEqual(requests.filter { $0.opcode == .select }.count, 1)
    }
    func testCancellationWhileProviderOpensClosesLateLeaseWithoutWriting() async throws {
        let backend = OTATransferMockTransport(); let session = APP16Session(backend)
        let provider = APP16Provider(session, delay: true)
        let controller = OTAInstallationController(provider: provider)
        let candidate = try candidate()
        let work = Task { try await controller.upload(candidate, transferID: 1) }
        while await provider.opens == 0 { await Task.yield() }
        await controller.cancel()
        do { try await work.value; XCTFail() } catch {}
        let snapshot = await controller.snapshot(); XCTAssertEqual(snapshot.state, .unknown)
        let requests = await backend.requests; XCTAssertTrue(requests.isEmpty)
        let closed = await session.closed; XCTAssertTrue(closed)
    }
    func testCancellationAfterDeliveryRejectsLateSuccessAndReadyCanBeAbandoned() async throws {
        let backend = OTATransferMockTransport(fault: .delay, at: .start)
        let session = APP16Session(backend); let controller = OTAInstallationController(provider: APP16Provider(session))
        let candidate = try candidate()
        let work = Task { try await controller.upload(candidate, transferID: 1) }
        while await backend.requests.isEmpty { await Task.yield() }
        work.cancel(); await controller.cancel()
        do { try await work.value; XCTFail() } catch {}
        let snapshot = await controller.snapshot(); XCTAssertEqual(snapshot.state, .unknown)
        let requests = await backend.requests; XCTAssertEqual(requests.count, 1)
        let nextBackend = OTATransferMockTransport(); let next = OTAInstallationController(provider: APP16Provider(APP16Session(nextBackend)))
        try await next.upload(candidate, transferID: 2); await next.cancel()
        let abandoned = await next.snapshot(); XCTAssertEqual(abandoned.state, .unknown); XCTAssertFalse(abandoned.selectionMayHaveChanged)
        let noSelection = await nextBackend.requests; XCTAssertFalse(noSelection.contains { $0.opcode == .select || $0.opcode == .cancel })
    }
    func testOpeningDeadlineIsBoundedWithoutAnyTransfer() async throws {
        let backend = OTATransferMockTransport()
        let provider = APP16Provider(APP16Session(backend), delay: true)
        let controller = OTAInstallationController(provider: provider, operationTimeout: .milliseconds(10))
        do { try await controller.upload(candidate(), transferID: 1); XCTFail() }
        catch { XCTAssertEqual(error as? OTATransferError, .timeout) }
        let state = await controller.snapshot(); XCTAssertEqual(state.state, .unknown)
        let requests = await backend.requests; XCTAssertTrue(requests.isEmpty)
    }
    func testProviderRejectionAndBusyDoNotOpenAnotherSession() async throws {
        let backend = OTATransferMockTransport(); let provider = APP16Provider(APP16Session(backend), reject: true)
        let controller = OTAInstallationController(provider: provider)
        do { try await controller.upload(candidate(), transferID: 1); XCTFail() } catch {}
        let state = await controller.snapshot(); XCTAssertEqual(state.state, .unknown)
        let requests = await backend.requests; XCTAssertTrue(requests.isEmpty)
        let delayed = APP16Provider(APP16Session(backend), delay: true)
        let next = OTAInstallationController(provider: delayed); let candidate = try candidate()
        let work = Task { try await next.upload(candidate, transferID: 1) }
        while await delayed.opens == 0 { await Task.yield() }
        do { try await next.select(); XCTFail() } catch { XCTAssertEqual(error as? OTATransferError, .busy) }
        do { try await next.upload(candidate, transferID: 2); XCTFail() } catch { XCTAssertEqual(error as? OTATransferError, .busy) }
        try await work.value
        let opens = await delayed.opens; XCTAssertEqual(opens, 1)
        await next.cancel()
    }
}
#endif

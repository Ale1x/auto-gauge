#if os(macOS)
import Foundation
@testable import GaugeCore

actor OTATransferMockTransport: OTATransferTransport {
    enum Fault: Sendable, CaseIterable {
        case wrongID, wrongPhase, wrongManifestOffset, wrongImageOffset, reservedFlag, trailing
        case rejected, generationChange, lostACK, delay, selectionFlag
    }
    enum Failure: Error { case lostACK }
    var generation: UInt64 = 41
    var requests: [OTATransferRequest] = []
    var manifest = Data()
    var image = Data()
    var phase: OTATransferPhase = .idle
    let fault: Fault?
    let faultOpcode: OTATransferOpcode
    init(fault: Fault? = nil, at opcode: OTATransferOpcode = .image, generation: UInt64 = 41) {
        self.fault = fault; faultOpcode = opcode; self.generation = generation
    }
    func currentGeneration() async throws -> UInt64 { generation }
    func exchange(_ payload: Data, generation: UInt64, deadline: ContinuousClock.Instant) async throws -> Data {
        guard generation == self.generation else { throw OTATransferError.generationChanged }
        let request = try OTATransferCodec.decodeRequest(payload); requests.append(request)
        switch request.opcode {
        case .start: phase = .manifest
        case .manifest: manifest.append(request.body)
        case .begin: phase = .writing
        case .image: image.append(request.body)
        case .finish: phase = .ready
        case .select: phase = .selected
        case .cancel: phase = .fault
        }
        let active = request.opcode == faultOpcode ? fault : nil
        if active == .delay { try await Task.sleep(for: .seconds(3)) }
        if active == .lostACK { throw Failure.lostACK }
        if active == .generationChange { self.generation += 1 }
        var result = try OTATransferCodec.encodeResponse(.init(status: active == .rejected ? .backendFailure : .ok,
            phase: active == .wrongPhase ? .fault : phase,
            selectionMayHaveChanged: active == .selectionFlag ? request.opcode != .select : request.opcode == .select,
            transferID: active == .wrongID ? request.transferID &+ 1 : request.transferID,
            manifestOffset: UInt32(manifest.count) + (active == .wrongManifestOffset ? 1 : 0),
            imageOffset: UInt32(image.count) + (active == .wrongImageOffset ? 1 : 0)))
        if active == .reservedFlag { result[3] = 2 }
        if active == .trailing { result.append(0) }
        return result
    }
}
#endif

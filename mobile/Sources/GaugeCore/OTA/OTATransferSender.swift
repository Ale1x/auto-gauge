import Foundation

/// Host-development sender. Upload stops at READY; selecting boot is a separate call.
/// No retry/resume/reset exists: uncertain delivery permanently closes this sender.
public actor OTATransferSender {
    private let transport: any OTATransferTransport
    private let allowHostDevelopmentMutations: Bool
    private let operationTimeout: Duration
    private var busy = false
    private var generation: UInt64?
    private var transferID: UInt32 = 0
    public private(set) var state: OTATransferSenderState = .idle
    public private(set) var manifestBytesAcknowledged: UInt32 = 0
    public private(set) var imageBytesAcknowledged: UInt32 = 0
    public private(set) var selectionMayHaveChanged = false

    public init(transport: any OTATransferTransport, allowHostDevelopmentMutations: Bool = false,
                operationTimeout: Duration = .seconds(60)) {
        self.transport = transport; self.allowHostDevelopmentMutations = allowHostDevelopmentMutations
        self.operationTimeout = operationTimeout
    }
    public func upload(_ candidate: VerifiedOTACandidate, transferID: UInt32) async throws {
        try preflight()
        guard state == .idle else { throw state == .unknown ? OTATransferError.outcomeUnknown : OTATransferError.notReady }
        guard transferID != 0, !candidate.envelopeSnapshot.isEmpty,
              candidate.envelopeSnapshot.count <= Int(OTATransferCodec.maximumManifestBytes),
              candidate.imageSnapshot.count == candidate.manifest.imageBytes else { throw OTATransferError.invalidPayload }
        try Task.checkCancellation()
        busy = true; defer { busy = false }
        var attempted = false
        let deadline = ContinuousClock().now.advanced(by: operationTimeout)
        do {
            let transport = self.transport
            let current = try await Self.until(deadline) { try await transport.currentGeneration() }
            guard current != 0 else { throw OTATransferError.notReady }
            try Task.checkCancellation()
            generation = current; self.transferID = transferID; state = .uploading
            attempted = true
            try await send(.init(opcode: .start, transferID: transferID, offset: UInt32(candidate.envelopeSnapshot.count)),
                           phase: .manifest, manifestOffset: 0, imageOffset: 0, deadline: deadline)
            var offset = 0
            while offset < candidate.envelopeSnapshot.count {
                let end = min(candidate.envelopeSnapshot.count, offset + OTATransferCodec.maximumChunkBytes)
                try await send(.init(opcode: .manifest, transferID: transferID, offset: UInt32(offset),
                                     body: candidate.envelopeSnapshot.subdata(in: offset..<end)),
                               phase: .manifest, manifestOffset: UInt32(end), imageOffset: 0, deadline: deadline)
                offset = end
            }
            try await send(.init(opcode: .begin, transferID: transferID), phase: .writing,
                           manifestOffset: UInt32(candidate.envelopeSnapshot.count), imageOffset: 0, deadline: deadline)
            offset = 0
            while offset < candidate.imageSnapshot.count {
                let end = min(candidate.imageSnapshot.count, offset + OTATransferCodec.maximumChunkBytes)
                try await send(.init(opcode: .image, transferID: transferID, offset: UInt32(offset),
                                     body: candidate.imageSnapshot.subdata(in: offset..<end)),
                               phase: .writing, manifestOffset: UInt32(candidate.envelopeSnapshot.count), imageOffset: UInt32(end), deadline: deadline)
                offset = end
            }
            try await send(.init(opcode: .finish, transferID: transferID), phase: .ready,
                           manifestOffset: UInt32(candidate.envelopeSnapshot.count), imageOffset: UInt32(candidate.imageSnapshot.count), deadline: deadline)
            state = .ready
        } catch {
            if attempted { state = .unknown }
            throw error
        }
    }
    public func select() async throws {
        try preflight()
        guard state == .ready else { throw state == .unknown ? OTATransferError.outcomeUnknown : OTATransferError.notReady }
        try Task.checkCancellation()
        busy = true; defer { busy = false }
        // Conservative before any await: a failure never licenses another select.
        selectionMayHaveChanged = true
        do {
            try await send(.init(opcode: .select, transferID: transferID), phase: .selected,
                           manifestOffset: manifestBytesAcknowledged, imageOffset: imageBytesAcknowledged,
                           deadline: ContinuousClock().now.advanced(by: operationTimeout))
            state = .selected
        } catch { state = .unknown; throw error }
    }
    private func preflight() throws {
        guard allowHostDevelopmentMutations else { throw OTATransferError.unqualified }
        guard !busy else { throw OTATransferError.busy }
        guard operationTimeout > .zero, operationTimeout <= .seconds(15 * 60) else { throw OTATransferError.invalidTimeout }
    }
    private func send(_ request: OTATransferRequest, phase: OTATransferPhase,
                      manifestOffset: UInt32, imageOffset: UInt32, deadline: ContinuousClock.Instant) async throws {
        try Task.checkCancellation()
        guard let generation else { throw OTATransferError.notReady }
        let transport = self.transport
        let before = try await Self.until(deadline) { try await transport.currentGeneration() }
        guard before == generation else { throw OTATransferError.generationChanged }
        try Task.checkCancellation()
        let payload = try OTATransferCodec.encodeRequest(request)
        let responseBytes = try await Self.until(deadline) { try await transport.exchange(payload, generation: generation, deadline: deadline) }
        try Task.checkCancellation()
        let after = try await Self.until(deadline) { try await transport.currentGeneration() }
        guard after == generation else { throw OTATransferError.generationChanged }
        try Task.checkCancellation()
        let response: OTATransferResponse
        do { response = try OTATransferCodec.decodeResponse(responseBytes) }
        catch { throw OTATransferError.invalidReply }
        if response.selectionMayHaveChanged { selectionMayHaveChanged = true }
        guard response.status == .ok else { throw OTATransferError.rejected(response.status) }
        guard response.transferID == transferID, response.phase == phase,
              response.manifestOffset == manifestOffset, response.imageOffset == imageOffset,
              response.selectionMayHaveChanged == (request.opcode == .select) else { throw OTATransferError.invalidReply }
        manifestBytesAcknowledged = manifestOffset; imageBytesAcknowledged = imageOffset
    }
    private static func until<T: Sendable>(_ deadline: ContinuousClock.Instant,
                                           operation: @escaping @Sendable () async throws -> T) async throws -> T {
        guard ContinuousClock().now < deadline else { throw OTATransferError.timeout }
        return try await withThrowingTaskGroup(of: T.self) { group in
            group.addTask { try await operation() }
            group.addTask { try await ContinuousClock().sleep(until: deadline); throw OTATransferError.timeout }
            defer { group.cancelAll() }
            guard let result = try await group.next(), ContinuousClock().now < deadline else { throw OTATransferError.timeout }
            return result
        }
    }
}

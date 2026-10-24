import Foundation

/// AG-framed in-memory transport. The real transport remains disabled.
public actor SimulatedGaugeTransport: GaugeTransport {
    private let channel: any SimulatedPacketChannel
    private let clientID: String
    private var lease: SimulatedConnectionLease?
    private var nextRequest: UInt64 = 1
    private var inFlight = false
    private var catalog: [PIDDescriptor] = []
    private var lastObserved: GaugeConfiguration?
    private var mustRefresh = true
    private var state: SimulatedCommitState = .idle
    private var phases: [String] = []
    public init(channel: any SimulatedPacketChannel, clientID: String) { self.channel = channel; self.clientID = clientID }
    public init() {
        do { channel = try InMemoryGaugeEndpoint() }
        catch { fatalError("La configurazione dimostrativa incorporata non è valida: \(error)") }
        clientID = "IPHONE-SIM-A"
    }
    public func connect() async throws {
        guard !inFlight else { throw SimulatedLinkError.operationInProgress }
        if lease != nil { return }
        inFlight = true
        defer { inFlight = false }
        let connected = try await channel.connect(clientID: clientID)
        do { try Task.checkCancellation() }
        catch { await channel.disconnect(lease: connected); throw error }
        lease = connected
        catalog = []
        mustRefresh = true
        if state != .unknown { state = .idle }
    }
    public func disconnect() async {
        guard let old = lease else { return }
        lease = nil; mustRefresh = true
        await channel.disconnect(lease: old)
    }
    public func isConnected() -> Bool { lease != nil }
    public func requiresRead() -> Bool { mustRefresh }
    public func lastObservedConfiguration() -> GaugeConfiguration? { lastObserved }
    public func commitState() -> SimulatedCommitState { state }
    public func lastApplyPhases() -> [String] { phases }
    public func supportedPIDs() async throws -> [PIDDescriptor] {
        let reply = try await single(.init(op: "hello"))
        guard reply.op == "hello", reply.protocolVersion == 1 else { throw SimulatedLinkError.incompatibleVersion }
        guard reply.deviceID == "SIMULATO-001", reply.authenticated == false,
              let numbers = reply.supportedService01PIDs, !numbers.isEmpty, numbers.count <= 255,
              Set(numbers).count == numbers.count, numbers.allSatisfy({ (1...255).contains($0) }) else { throw SimulatedLinkError.invalidReply }
        let metadata = InMemoryGaugeEndpoint.catalog
        let candidate = numbers.map { number in metadata.first(where: { $0.pid == number }) ?? PIDDescriptor(pid: number, ecuSource: "SIMULATO-ECU", nameIT: "PID Service 01 \(number)", unit: "") }
        catalog = candidate
        return candidate
    }
    public func configuration() async throws -> GaugeConfiguration {
        do {
            if catalog.isEmpty { _ = try await supportedPIDs() }
            let reply = try await single(.init(op: "config.get"))
            guard reply.phase == "current", reply.persistence == "volatile", let configuration = reply.configuration else { throw SimulatedLinkError.invalidReply }
            try ConfigurationValidator.validate(configuration, catalog: catalog)
            lastObserved = configuration; mustRefresh = false
            if state == .unknown { state = .idle }
            return configuration
        } catch { mustRefresh = true; throw error }
    }
    public func validateDraft(_ draft: GaugeConfiguration) async throws {
        if catalog.isEmpty { _ = try await supportedPIDs() }
        try ConfigurationValidator.validate(draft, catalog: catalog)
        let reply = try await single(.init(op: "config.validate", configuration: draft))
        guard reply.phase == "validated" else { throw SimulatedLinkError.invalidReply }
    }
    public func apply(_ configuration: GaugeConfiguration) async throws { _ = try await applyDraft(configuration) }
    @discardableResult
    public func applyDraft(_ draft: GaugeConfiguration, onPhase: @Sendable (SimulatedCommitState) async -> Void = { _ in }) async throws -> GaugeConfiguration {
        guard !inFlight else { throw SimulatedLinkError.operationInProgress }
        guard let sentLease = lease else { throw SimulatedLinkError.disconnected }
        guard !mustRefresh, let base = lastObserved else { throw SimulatedLinkError.mustRead }
        guard base.revision < 2_147_483_647 else { throw SimulatedLinkError.exhausted }
        var candidate = draft
        candidate.revision = base.revision + 1
        try ConfigurationValidator.validate(candidate, catalog: catalog)
        let request = try encoded(.init(op: "config.apply", configuration: candidate, expectedRevision: base.revision))
        inFlight = true; phases = []
        defer { inFlight = false }
        var finishAttempted = false, knownRejected = false
        do {
            let batch = try await channel.begin(packet: request.packet, lease: sentLease)
            guard lease == sentLease, batch.hasContinuation, batch.packets.count == 1 else { throw SimulatedLinkError.invalidReply }
            let received = try decoded(batch.packets[0], requestID: request.id)
            guard received.phase == "received", received.error == nil, received.appliedRevision == nil, received.persistence == nil else { throw SimulatedLinkError.invalidReply }
            phases.append("received"); state = .received
            await onPhase(state)
            guard lease == sentLease else { throw SimulatedLinkError.disconnected }
            finishAttempted = true
            let packets = try await channel.finish(requestID: request.id, lease: sentLease)
            guard lease == sentLease, packets.count == 1 else { throw SimulatedLinkError.invalidReply }
            let applied = try decoded(packets[0], requestID: request.id)
            if applied.phase == "rejected" {
                guard let reason = applied.error, !reason.isEmpty else { throw SimulatedLinkError.invalidReply }
                knownRejected = true
                phases.append("rejected"); state = .rejected(reason)
                if reason == "revision_conflict" { mustRefresh = true }
                await onPhase(state)
                throw rejected(reason)
            }
            guard applied.phase == "applied", applied.appliedRevision == candidate.revision, applied.persistence == "volatile" else { throw SimulatedLinkError.invalidReply }
            phases.append("applied"); state = .appliedVolatile(candidate.revision)
            lastObserved = candidate; mustRefresh = false
            await onPhase(state)
            return candidate
        } catch {
            if !knownRejected {
                mustRefresh = true
                state = finishAttempted ? .unknown : .rejected("delivery_failed_before_commit")
                if lease == sentLease { lease = nil }
                await channel.disconnect(lease: sentLease)
                await onPhase(state)
                if finishAttempted { throw SimulatedLinkError.unknownCommit }
            }
            throw error
        }
    }
    public func readLogs(sessionID: LogSessionID, after: UInt64, limit: Int = 16) async throws -> HistoricalLogPage {
        guard (1...16).contains(limit) else { throw LogImportError.invalidPage }
        let reply = try await single(.init(op: "logs.read", sessionID: sessionID.value, afterSequence: .init(after), limit: limit))
        guard reply.op == "logs.read", let page = reply.page, page.sessionID == sessionID,
              page.records.count <= limit, page.records.allSatisfy({ $0.sequence.value > after }),
              page.lost.allSatisfy({ $0.first.value > after }) else { throw SimulatedLinkError.invalidReply }
        return page
    }
    public func acknowledgeLogs(_ acknowledgement: HistoricalLogAcknowledgement) async throws {
        let reply = try await single(.init(op: "logs.ack", sessionID: acknowledgement.sessionID.value, acknowledgedThrough: acknowledgement.acknowledgedThrough))
        guard reply.op == "logs.ack", reply.sessionID == acknowledgement.sessionID.value,
              reply.acknowledgedThrough == acknowledgement.acknowledgedThrough, reply.sourceRecordsDeleted == false else { throw SimulatedLinkError.invalidReply }
    }
    public func importHistoryPage(sessionID: LogSessionID, archive: LocalLogArchive, limit: Int = 16) async throws -> SimulatedLogImportResult {
        let cursor = await archive.cursor(deviceID: "SIMULATO-001", sessionID: sessionID.value)
        let page = try await readLogs(sessionID: sessionID, after: cursor, limit: limit)
        let acknowledgement = try await archive.importPage(deviceID: "SIMULATO-001", data: JSONEncoder().encode(page))
        do {
            try await acknowledgeLogs(acknowledgement)
            return .init(acknowledgement: acknowledgement, peerAcknowledged: true, warning: nil)
        } catch {
            return .init(acknowledgement: acknowledgement, peerAcknowledged: false, warning: "Import locale salvato; conferma al simulatore non recapitata. Nessun record sorgente cancellato. \(error.localizedDescription)")
        }
    }
    private func single(_ request: GaugeEndpointRequest) async throws -> GaugeEndpointReply {
        guard !inFlight else { throw SimulatedLinkError.operationInProgress }
        guard let sentLease = lease else { throw SimulatedLinkError.disconnected }
        let wire = try encoded(request)
        inFlight = true
        defer { inFlight = false }
        let batch = try await channel.begin(packet: wire.packet, lease: sentLease)
        guard lease == sentLease, !batch.hasContinuation, batch.packets.count == 1 else { throw SimulatedLinkError.invalidReply }
        let reply = try decoded(batch.packets[0], requestID: wire.id)
        if reply.phase == "rejected" { throw rejected(reply.error ?? "invalid_reply") }
        return reply
    }
    private func encoded(_ request: GaugeEndpointRequest) throws -> (id: UInt32, packet: Data) {
        guard nextRequest <= UInt64(UInt32.max) else { throw SimulatedLinkError.exhausted }
        let id = UInt32(nextRequest)
        nextRequest += 1
        return (id, try GaugeFrameCodec.encode(.init(kind: .request, requestID: id, payload: JSONEncoder().encode(request))))
    }
    private func decoded(_ packet: Data, requestID: UInt32) throws -> GaugeEndpointReply {
        let frame = try GaugeFrameCodec.decode(packet)
        guard frame.kind == .response, frame.requestID == requestID else { throw SimulatedLinkError.invalidReply }
        let reply = try JSONDecoder().decode(GaugeEndpointReply.self, from: frame.payload)
        guard reply.simulation else { throw SimulatedLinkError.invalidReply }
        return reply
    }
    private func rejected(_ reason: String) -> SimulatedLinkError {
        reason == "revision_conflict" ? .revisionConflict : .rejected(reason)
    }
}

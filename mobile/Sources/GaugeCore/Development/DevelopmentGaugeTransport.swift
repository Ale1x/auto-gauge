import Foundation

/// One transport for explicitly local development pipes (in-memory endpoint or host C process).
/// Does not accept the native Bluetooth pipe, and does not establish authentication.
public actor DevelopmentGaugeTransport: GaugeTransport {
    private let pipe: any DevelopmentGaugePacketPipe
    private let client: GaugeRPCClient
    private var catalog: [PIDDescriptor] = []
    private var deviceID: String?
    private var observed: GaugeConfiguration?
    private var persistence: String?
    private var mustRead = true
    private var state: GaugeCommitState = .idle
    private var busy = false
    private var epoch: UInt64 = 0
    public init(pipe: any DevelopmentGaugePacketPipe, persistencePolicy: GaugeRPCPersistencePolicy = .volatileDevelopment, requestTimeout: Duration = .seconds(30)) {
        self.pipe = pipe
        client = GaugeRPCClient(pipe: pipe, allowTestMutations: true, requestTimeout: requestTimeout, persistencePolicy: persistencePolicy)
    }
    public func connect() async throws {
        guard !busy else { throw GaugeLinkError.busy }
        busy = true; defer { busy = false }
        let token = epoch
        if await client.isConnected() {
            guard token == epoch else { throw GaugeLinkError.disconnected }; return
        }
        do {
            try await pipe.connect()
            try Task.checkCancellation()
            guard token == epoch else { throw GaugeLinkError.disconnected }
            try await client.open()
            guard token == epoch else { throw GaugeLinkError.disconnected }
        } catch {
            await client.disconnect()
            if let lease = try? await pipe.readyConnection() { await pipe.disconnect(generation: lease.generation) }
            throw error
        }
        catalog = []; deviceID = nil; mustRead = true
        if state != .unknown { state = .idle }
    }
    public func disconnect() async { epoch &+= 1; mustRead = true; await client.disconnect() }
    public func isConnected() async -> Bool { await client.isConnected() }
    public func requiresRead() -> Bool { mustRead }
    public func commitState() -> GaugeCommitState { state }
    public func lastObservedConfiguration() -> GaugeConfiguration? { observed }
    public func lastPersistence() -> String? { persistence }
    public func lastEndpointFailure() async -> GaugeEndpointFailure? { await client.lastEndpointFailure }
    public func deviceIdentifier() throws -> String {
        guard let deviceID else { throw GaugeLinkError.notReady }; return deviceID
    }
    public func supportedPIDs() async throws -> [PIDDescriptor] {
        guard !busy else { throw GaugeLinkError.busy }
        busy = true; defer { busy = false }
        return try await hello(token: epoch)
    }
    private func hello(token: UInt64) async throws -> [PIDDescriptor] {
        let reply = try await client.request(.hello, body: JSONEncoder().encode(GaugeEndpointRequest(op: "hello")))
        guard token == epoch else { throw GaugeLinkError.disconnected }
        guard let numbers = reply.supportedService01PIDs, let id = reply.deviceID,
              (1...64).contains(id.unicodeScalars.count), id.utf8.count <= 256 else { throw GaugeLinkError.invalidReply }
        catalog = numbers.map { pid in
            let metadata = DemoData.catalog.first(where: { $0.pid == pid })
            return PIDDescriptor(pid: pid, nameIT: metadata?.nameIT ?? "PID Service 01 \(String(pid, radix: 16).uppercased())", unit: metadata?.unit ?? "", supported: true)
        }
        deviceID = id; return catalog
    }
    public func configuration() async throws -> GaugeConfiguration {
        guard !busy else { throw GaugeLinkError.busy }
        busy = true; defer { busy = false }
        let token = epoch
        do {
            if catalog.isEmpty { _ = try await hello(token: token) }
            let reply = try await client.request(.configuration, body: JSONEncoder().encode(GaugeEndpointRequest(op: "config.get")))
            guard token == epoch else { throw GaugeLinkError.disconnected }
            guard let config = reply.configuration else { throw GaugeLinkError.invalidReply }
            let result = try normalized(config)
            observed = result; persistence = reply.persistence; mustRead = false
            if state == .unknown { state = .idle }
            return result
        } catch { mustRead = true; if error is GaugeEndpointFailure { state = .unknown }; throw error }
    }
    public func validateDraft(_ draft: GaugeConfiguration) async throws {
        guard !busy else { throw GaugeLinkError.busy }
        busy = true; defer { busy = false }
        let token = epoch
        if catalog.isEmpty { _ = try await hello(token: token) }
        try ConfigurationValidator.validate(draft, catalog: catalog)
        _ = try await client.request(.validate, body: JSONEncoder().encode(GaugeEndpointRequest(op: "config.validate", configuration: draft)))
        guard token == epoch else { throw GaugeLinkError.disconnected }
    }
    public func apply(_ configuration: GaugeConfiguration) async throws { _ = try await applyDraft(configuration) }
    @discardableResult
    public func applyDraft(_ draft: GaugeConfiguration, onPhase: @Sendable (GaugeCommitState) async -> Void = { _ in }) async throws -> GaugeConfiguration {
        guard !busy else { throw GaugeLinkError.busy }
        guard !mustRead, let observed, observed.revision < 2_147_483_647 else { throw GaugeLinkError.notReady }
        var candidate = draft; candidate.revision = observed.revision + 1
        candidate = try normalized(candidate)
        busy = true; defer { busy = false }
        let token = epoch
        do {
            let reply = try await client.request(.apply, body: JSONEncoder().encode(GaugeEndpointRequest(op: "config.apply", configuration: candidate, expectedRevision: observed.revision)), expectedAppliedRevision: candidate.revision, onReceived: { [weak self] in
                if await self?.recordReceived(token: token) == true { await onPhase(.received) }
            })
            guard token == epoch else { throw GaugeLinkError.unknownCommit }
            self.observed = candidate; persistence = reply.persistence; mustRead = false
            state = reply.persistence == "durable" ? .appliedHostDurable(candidate.revision) : .appliedVolatile(candidate.revision)
            await onPhase(state); return candidate
        } catch {
            mustRead = true
            if await client.applyOutcomeUnknown { state = .unknown }
            else if case GaugeLinkError.rejected(let reason) = error { state = .rejected(reason) }
            else { state = .rejected("delivery_failed_before_final_chunk") }
            await onPhase(state); throw error
        }
    }
    private func recordReceived(token: UInt64) -> Bool {
        guard token == epoch else { return false }; state = .received; return true
    }
    private func normalized(_ config: GaugeConfiguration) throws -> GaugeConfiguration {
        try ConfigurationValidator.validate(config, catalog: catalog)
        var result = config
        for dashboard in result.dashboards.indices {
            for widget in result.dashboards[dashboard].widgets.indices {
                let pid = result.dashboards[dashboard].widgets[widget].pid.pid
                guard let descriptor = catalog.first(where: { $0.pid == pid }) else { throw GaugeLinkError.invalidReply }
                result.dashboards[dashboard].widgets[widget].pid = descriptor
            }
        }
        return result
    }
    public func readLogs(sessionID: LogSessionID, after: UInt64, limit: Int = 16) async throws -> HistoricalLogPage {
        guard !busy else { throw GaugeLinkError.busy }
        busy = true; defer { busy = false }
        return try await readLogPage(sessionID: sessionID, after: after, limit: limit, token: epoch)
    }
    private func readLogPage(sessionID: LogSessionID, after: UInt64, limit: Int, token: UInt64) async throws -> HistoricalLogPage {
        guard token == epoch else { throw GaugeLinkError.disconnected }
        let reply = try await client.request(.logs, body: JSONEncoder().encode(GaugeEndpointRequest(op: "logs.read", sessionID: sessionID.value, afterSequence: .init(after), limit: limit)))
        guard token == epoch else { throw GaugeLinkError.disconnected }
        guard let page = reply.page else { throw GaugeLinkError.invalidReply }; return page
    }
    public func acknowledgeLogs(_ acknowledgement: HistoricalLogAcknowledgement) async throws {
        guard !busy else { throw GaugeLinkError.busy }
        busy = true; defer { busy = false }
        try await sendAcknowledgement(acknowledgement, token: epoch)
    }
    private func sendAcknowledgement(_ ack: HistoricalLogAcknowledgement, token: UInt64) async throws {
        guard token == epoch else { throw GaugeLinkError.disconnected }
        _ = try await client.request(.acknowledge, body: JSONEncoder().encode(GaugeEndpointRequest(op: "logs.ack", sessionID: ack.sessionID.value, acknowledgedThrough: ack.acknowledgedThrough)))
        guard token == epoch else { throw GaugeLinkError.disconnected }
    }
    public func importHistoryPage(sessionID: LogSessionID, archive: LocalLogArchive, limit: Int = 16) async throws -> GaugeLogImportResult {
        guard !busy else { throw GaugeLinkError.busy }
        busy = true; defer { busy = false }
        let token = epoch
        let id = try deviceIdentifier()
        let cursor = await archive.cursor(deviceID: id, sessionID: sessionID.value)
        guard token == epoch else { throw GaugeLinkError.disconnected }
        let page = try await readLogPage(sessionID: sessionID, after: cursor, limit: limit, token: token)
        guard token == epoch else { throw GaugeLinkError.disconnected }
        let ack = try await archive.importPage(deviceID: id, data: JSONEncoder().encode(page))
        do {
            try await sendAcknowledgement(ack, token: token)
            return .init(acknowledgement: ack, peerAcknowledged: true, warning: nil)
        } catch {
            return .init(acknowledgement: ack, peerAcknowledged: false, warning: "Storico salvato sul telefono; ACK non recapitato. \(error.localizedDescription)")
        }
    }
}

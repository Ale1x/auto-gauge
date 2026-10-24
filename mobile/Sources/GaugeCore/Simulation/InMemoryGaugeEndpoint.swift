import Foundation

public actor InMemoryGaugeEndpoint: SimulatedPacketChannel {
    public static let supportedPIDs = [4, 5, 12, 13, 15, 16, 17]
    public static var catalog: [PIDDescriptor] {
        supportedPIDs.map { number in
            let metadata = DemoData.catalog.first { $0.pid == number }
            return .init(pid: number, ecuSource: "SIMULATO-ECU", nameIT: metadata?.nameIT ?? "PID Service 01 \(String(number, radix: 16).uppercased())", unit: metadata?.unit ?? "")
        }
    }
    private var current: GaugeConfiguration
    private let historySource: SimulatedHistorySource?
    private var active: SimulatedConnectionLease?
    private var nextLease: UInt64 = 1
    private struct Pending: Sendable { let lease: SimulatedConnectionLease; let requestID: UInt32; let candidate: GaugeConfiguration?; let expected: Int?; let rejection: String? }
    private var pending: Pending?
    public init(configuration: GaugeConfiguration = DemoData.configuration, historySource: SimulatedHistorySource? = nil) throws {
        try ConfigurationValidator.validate(configuration, catalog: Self.catalog)
        current = configuration
        self.historySource = historySource
    }
    public func simulatedConfiguration() -> GaugeConfiguration { current }
    public func connect(clientID: String) throws -> SimulatedConnectionLease {
        guard !clientID.isEmpty, clientID.utf8.count <= 64 else { throw SimulatedLinkError.invalidClient }
        guard active == nil else { throw SimulatedLinkError.busy }
        guard nextLease < UInt64.max else { throw SimulatedLinkError.exhausted }
        let lease = SimulatedConnectionLease(clientID: clientID, token: nextLease)
        nextLease += 1; active = lease
        return lease
    }
    public func disconnect(lease: SimulatedConnectionLease) {
        guard active == lease else { return }
        active = nil; pending = nil
    }
    public func begin(packet: Data, lease: SimulatedConnectionLease) throws -> SimulatedReplyBatch {
        try requireLease(lease)
        guard pending == nil else { throw SimulatedLinkError.operationInProgress }
        let frame = try GaugeFrameCodec.decode(packet)
        guard frame.kind == .request else { throw SimulatedLinkError.invalidReply }
        let request: GaugeEndpointRequest
        do {
            try StrictLogJSON.check(frame.payload)
            request = try JSONDecoder().decode(GaugeEndpointRequest.self, from: frame.payload)
        } catch { return try single(.init(phase: "rejected", error: "invalid_message"), requestID: frame.requestID) }
        switch request.op {
        case "hello":
            return try single(.init(op: "hello", deviceID: "SIMULATO-001", protocolVersion: 1, supportedService01PIDs: Self.supportedPIDs, authenticated: false), requestID: frame.requestID)
        case "config.get":
            return try single(.init(phase: "current", configuration: current, persistence: "volatile"), requestID: frame.requestID)
        case "logs.read", "logs.ack":
            guard let source = historySource else { return try single(.init(phase: "rejected", error: "history_unavailable"), requestID: frame.requestID) }
            guard request.sessionID == source.sessionID.value else { return try single(.init(phase: "rejected", error: "session_mismatch"), requestID: frame.requestID) }
            do {
                if request.op == "logs.ack" {
                    guard let through = request.acknowledgedThrough, through.value <= source.lastSequence else { throw LogImportError.invalidCounter }
                    return try single(.init(op: "logs.ack", sessionID: source.sessionID.value, acknowledgedThrough: through, sourceRecordsDeleted: false), requestID: frame.requestID)
                }
                guard let after = request.afterSequence, let limit = request.limit, (1...16).contains(limit), after.value <= source.lastSequence else { throw LogImportError.invalidPage }
                let lost = try source.losses(after: after.value)
                var records: [HistoricalLogRecord] = []
                for record in source.records where record.sequence.value > after.value {
                    let page = try HistoricalLogPage(sessionID: source.sessionID, records: records + [record], lost: lost)
                    if (try JSONEncoder().encode(GaugeEndpointReply(op: "logs.read", page: page))).count > GaugeFrameCodec.maximumPayload { break }
                    records.append(record)
                    if records.count == limit { break }
                }
                let page = try HistoricalLogPage(sessionID: source.sessionID, records: records, lost: lost)
                return try single(.init(op: "logs.read", page: page), requestID: frame.requestID)
            } catch { return try single(.init(phase: "rejected", error: "invalid_history_request"), requestID: frame.requestID) }
        case "config.validate", "config.apply":
            var rejection: String?
            if let configuration = request.configuration {
                do { try ConfigurationValidator.validate(configuration, catalog: Self.catalog) }
                catch ConfigurationError.unsupportedPID { rejection = "unsupported_pid" }
                catch { rejection = "invalid_configuration" }
            } else { rejection = "invalid_configuration" }
            if request.op == "config.validate" {
                return try single(rejection.map { .init(phase: "rejected", error: $0) } ?? .init(phase: "validated"), requestID: frame.requestID)
            }
            pending = Pending(lease: lease, requestID: frame.requestID, candidate: request.configuration, expected: request.expectedRevision, rejection: rejection)
            return SimulatedReplyBatch(packets: [try self.packet(.init(phase: "received"), requestID: frame.requestID)], hasContinuation: true)
        default:
            return try single(.init(phase: "rejected", error: "not_implemented"), requestID: frame.requestID)
        }
    }
    public func finish(requestID: UInt32, lease: SimulatedConnectionLease) throws -> [Data] {
        try requireLease(lease)
        guard let transaction = pending, transaction.lease == lease, transaction.requestID == requestID else { throw SimulatedLinkError.invalidReply }
        defer { pending = nil }
        if let rejection = transaction.rejection { return [try packet(.init(phase: "rejected", error: rejection), requestID: requestID)] }
        guard let candidate = transaction.candidate, let expected = transaction.expected,
              expected == current.revision, expected < 2_147_483_647, candidate.revision == expected + 1 else {
            return [try packet(.init(phase: "rejected", error: "revision_conflict"), requestID: requestID)]
        }
        // Both readback and final ACK must be encodable before the in-memory commit.
        _ = try packet(.init(phase: "current", configuration: candidate, persistence: "volatile"), requestID: requestID)
        let applied = try packet(.init(phase: "applied", persistence: "volatile", appliedRevision: candidate.revision), requestID: requestID)
        current = candidate
        return [applied]
    }
    private func requireLease(_ lease: SimulatedConnectionLease) throws {
        guard active == lease else { throw SimulatedLinkError.disconnected }
    }
    private func single(_ reply: GaugeEndpointReply, requestID: UInt32) throws -> SimulatedReplyBatch {
        .init(packets: [try packet(reply, requestID: requestID)], hasContinuation: false)
    }
    private func packet(_ reply: GaugeEndpointReply, requestID: UInt32) throws -> Data {
        try GaugeFrameCodec.encode(.init(kind: .response, requestID: requestID, payload: JSONEncoder().encode(reply)))
    }
}

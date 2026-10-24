import Foundation

/// Development RPC only. Native construction always keeps mutation gate closed.
public actor GaugeRPCClient {
    private let pipe: any GaugePacketPipe
    private let allowTestMutations: Bool
    private let requestTimeout: Duration
    private let frameTimeout: Duration
    private var connection: GaugePipeConnection?
    private var nextID: UInt64 = 1
    private var lastGeneration: UInt64?
    private var epoch: UInt64 = 0
    private var busy = false
    private var capabilities: [PIDDescriptor]?
    public private(set) var applyOutcomeUnknown = false
    public init(pipe: any GaugePacketPipe, allowTestMutations: Bool = false, requestTimeout: Duration = .seconds(30), frameTimeout: Duration = .seconds(10)) {
        self.pipe = pipe; self.allowTestMutations = allowTestMutations; self.requestTimeout = requestTimeout; self.frameTimeout = frameTimeout
    }
    public func open() async throws {
        guard !busy, connection == nil else { throw GaugeLinkError.busy }
        let token = epoch
        let ready = try await pipe.readyConnection()
        guard token == epoch, ready.maximumWriteBytes > 0, ready.generation != lastGeneration else { throw GaugeLinkError.notReady }
        connection = ready; lastGeneration = ready.generation; nextID = 1; capabilities = nil; epoch &+= 1
        // A reconnect alone never proves a lost apply did not commit.
    }
    public func disconnect() async {
        epoch &+= 1
        let old = connection; connection = nil
        if let old { await pipe.disconnect(generation: old.generation) }
    }
    public func request(_ operation: GaugeRPCOperation, body: Data, expectedAppliedRevision: Int? = nil) async throws -> GaugeRPCResponse {
        guard !busy else { throw GaugeLinkError.busy }
        guard !operation.mutates || allowTestMutations else { throw GaugeLinkError.unqualified }
        if operation == .apply && applyOutcomeUnknown { throw GaugeLinkError.unknownCommit }
        guard let connection else { throw GaugeLinkError.disconnected }
        if operation == .configuration { guard capabilities != nil else { throw GaugeLinkError.notReady } }
        guard nextID <= UInt64(UInt32.max) else { throw GaugeLinkError.exhausted }
        guard body.count <= GaugeFrameCodec.maximumPayload else { throw FrameCodecError.length }
        try StrictLogJSON.check(body)
        let request = try JSONDecoder().decode(GaugeEndpointRequest.self, from: body)
        guard request.op == operation.rawValue else { throw GaugeLinkError.invalidReply }
        if operation == .apply {
            guard let revision = expectedAppliedRevision, (1...2_147_483_647).contains(revision), request.configuration?.revision == revision,
                  let expected = request.expectedRevision, (1..<2_147_483_647).contains(expected), revision == expected + 1 else { throw GaugeLinkError.invalidReply }
        }
        if operation == .validate { guard request.configuration != nil else { throw GaugeLinkError.invalidReply } }
        if operation == .logs {
            guard let session = request.sessionID, (try? LogSessionID(session)) != nil, request.afterSequence != nil,
                  let limit = request.limit, (1...16).contains(limit) else { throw GaugeLinkError.invalidReply }
        }
        if operation == .acknowledge {
            guard let session = request.sessionID, (try? LogSessionID(session)) != nil, request.acknowledgedThrough != nil else { throw GaugeLinkError.invalidReply }
        }
        let id = UInt32(nextID); nextID += 1
        let packet = try GaugeFrameCodec.encode(.init(kind: .request, requestID: id, payload: body))
        let token = epoch
        let deadline = ContinuousClock().now.advanced(by: requestTimeout)
        let fragmentBytes = min(connection.maximumWriteBytes, GaugeStreamReassembler.maximumChunkBytes)
        busy = true; defer { busy = false }
        var finalWriteAttempted = false
        var knownRejected = false
        do {
            var offset = 0
            while offset < packet.count {
                try Task.checkCancellation()
                guard token == epoch else { throw GaugeLinkError.disconnected }
                let end = min(packet.count, offset + fragmentBytes)
                let chunk = packet.subdata(in: offset..<end)
                if end == packet.count { finalWriteAttempted = true }
                try await Self.until(deadline) { try await self.pipe.write(chunk, generation: connection.generation) }
                offset = end
            }
            var decoder = GaugeStreamReassembler(frameTimeout: frameTimeout)
            var received = false
            while true {
                try Task.checkCancellation()
                guard token == epoch else { throw GaugeLinkError.disconnected }
                try decoder.checkTimeout(at: ContinuousClock().now)
                let readDeadline = min(deadline, decoder.deadline ?? deadline)
                let chunk = try await Self.until(readDeadline) { try await self.pipe.receive(generation: connection.generation) }
                guard token == epoch, chunk.receivedAt <= ContinuousClock().now else { throw GaugeLinkError.invalidReply }
                let frames = try decoder.feed(chunk.data, at: chunk.receivedAt)
                try decoder.checkTimeout(at: ContinuousClock().now)
                var final: GaugeRPCResponse?
                var rejection: String?
                for frame in frames {
                    guard final == nil, rejection == nil, frame.kind == .response, frame.requestID == id else { throw GaugeLinkError.invalidReply }
                    try StrictLogJSON.check(frame.payload)
                    let reply = try JSONDecoder().decode(GaugeRPCResponse.self, from: frame.payload)
                    try Self.checkOperation(reply, operation: operation, payload: frame.payload)
                    if let error = reply.error {
                        guard !error.isEmpty, (reply.phase == nil || reply.phase == "rejected"), reply.appliedRevision == nil, reply.persistence == nil else { throw GaugeLinkError.invalidReply }
                        rejection = error
                        continue
                    }
                    if operation == .apply {
                        if !received {
                            guard reply.phase == "received", reply.appliedRevision == nil, reply.persistence == nil else { throw GaugeLinkError.invalidReply }
                            received = true
                        } else {
                            guard reply.phase == "applied", reply.appliedRevision == expectedAppliedRevision, reply.persistence == "volatile" else { throw GaugeLinkError.invalidReply }
                            final = reply
                        }
                    } else {
                        switch operation {
                        case .hello:
                            guard reply.protocolVersion == 1, reply.phase == nil,
                                  let pids = reply.supportedService01PIDs, pids.count <= 255, Set(pids).count == pids.count,
                                  pids.allSatisfy({ (1...255).contains($0) }) else { throw GaugeLinkError.invalidReply }
                        case .configuration:
                            guard reply.phase == "current", let config = reply.configuration, reply.persistence == "volatile", let capabilities else { throw GaugeLinkError.invalidReply }
                            try ConfigurationValidator.validate(config, catalog: capabilities)
                        case .validate: guard reply.phase == "validated" else { throw GaugeLinkError.invalidReply }
                        case .logs: guard let page = reply.page, page.sessionID.value == request.sessionID,
                            let after = request.afterSequence, let limit = request.limit, (1...16).contains(limit), page.records.count <= limit,
                            page.records.allSatisfy({ $0.sequence > after }), page.lost.allSatisfy({ $0.first > after }) else { throw GaugeLinkError.invalidReply }
                        case .acknowledge: guard reply.sessionID == request.sessionID, reply.acknowledgedThrough == request.acknowledgedThrough, reply.sourceRecordsDeleted == false else { throw GaugeLinkError.invalidReply }
                        case .apply: break
                        }
                        final = reply
                    }
                }
                if let rejection {
                    guard decoder.bufferedBytes == 0 else { throw GaugeLinkError.invalidReply }
                    // Only contractually pre-commit failures resolve an apply definitively.
                    knownRejected = operation != .apply || ["revision_conflict", "invalid_configuration"].contains(rejection)
                    throw GaugeLinkError.rejected(rejection)
                }
                if let final {
                    guard decoder.bufferedBytes == 0 else { throw GaugeLinkError.invalidReply }
                    if operation == .hello, let pids = final.supportedService01PIDs {
                        capabilities = pids.map { PIDDescriptor(pid: $0, nameIT: "PID \($0)", unit: "", supported: true) }
                    }
                    var result = final
                    if operation == .configuration, var config = result.configuration, let capabilities {
                        for dashboard in config.dashboards.indices {
                            for widget in config.dashboards[dashboard].widgets.indices {
                                let pid = config.dashboards[dashboard].widgets[widget].pid.pid
                                guard let descriptor = capabilities.first(where: { $0.pid == pid }) else { throw GaugeLinkError.invalidReply }
                                config.dashboards[dashboard].widgets[widget].pid = descriptor
                            }
                        }
                        result.configuration = config
                    }
                    if operation == .apply || operation == .configuration { applyOutcomeUnknown = false }
                    return result
                }
            }
        } catch {
            if knownRejected, case GaugeLinkError.rejected = error {
                // A valid application rejection completes this request; the link/ID sequence remains usable.
                throw error
            }
            if operation == .apply && finalWriteAttempted && !knownRejected { applyOutcomeUnknown = true }
            if token == epoch { self.connection = nil; epoch &+= 1 }
            await pipe.disconnect(generation: connection.generation)
            if operation == .apply && finalWriteAttempted && !knownRejected { throw GaugeLinkError.unknownCommit }
            throw error
        }
    }
    private static func checkOperation(_ reply: GaugeRPCResponse, operation: GaugeRPCOperation, payload: Data) throws {
        if let op = reply.op {
            guard op == operation.rawValue else { throw GaugeLinkError.invalidReply }
            return
        }
        // Legacy responses omit op only for these contract-defined shapes. The frame
        // ID and pending operation/phase still correlate every such response.
        let allowed: Set<String>
        if reply.phase == "rejected", reply.error != nil {
            allowed = ["simulation", "phase", "error"]
        } else {
            switch (operation, reply.phase) {
            case (.configuration, "current"): allowed = ["simulation", "phase", "configuration", "persistence"]
            case (.validate, "validated"), (.apply, "received"): allowed = ["simulation", "phase"]
            case (.apply, "applied"): allowed = ["simulation", "phase", "applied_revision", "persistence"]
            default: throw GaugeLinkError.invalidReply
            }
        }
        guard let object = try JSONSerialization.jsonObject(with: payload) as? [String: Any],
              Set(object.keys).isSubset(of: allowed) else { throw GaugeLinkError.invalidReply }
    }
    private static func until<T: Sendable>(_ deadline: ContinuousClock.Instant, operation: @escaping @Sendable () async throws -> T) async throws -> T {
        guard ContinuousClock().now < deadline else { throw GaugeLinkError.timeout }
        return try await withThrowingTaskGroup(of: T.self) { group in
            group.addTask { try await operation() }
            group.addTask { try await ContinuousClock().sleep(until: deadline); throw GaugeLinkError.timeout }
            defer { group.cancelAll() }
            guard let result = try await group.next() else { throw GaugeLinkError.timeout }
            return result
        }
    }
}

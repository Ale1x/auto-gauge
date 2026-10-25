import Foundation

/// Development-only exclusive OTA pipe session. No scan, pairing, routing or radio
/// authorization is established here; native callers keep the mutation gate closed.
public actor OTAPacketTransport: OTAInstallationSession {
    private let pipe: any GaugePacketPipe
    private let allowHostDevelopmentMutations: Bool
    private let frameTimeout: Duration
    private var connection: GaugePipeConnection?
    private var opened = false
    private var closed = false
    private var busy = false
    private var epoch: UInt64 = 0
    private var nextID: UInt64 = 1

    public init(pipe: any GaugePacketPipe, allowHostDevelopmentMutations: Bool = false,
                frameTimeout: Duration = .seconds(10)) {
        self.pipe = pipe; self.allowHostDevelopmentMutations = allowHostDevelopmentMutations
        self.frameTimeout = frameTimeout
    }
    public func open(expectedConnection: GaugePipeConnection? = nil) async throws {
        guard allowHostDevelopmentMutations else { throw OTATransferError.unqualified }
        guard frameTimeout > .zero, frameTimeout <= .seconds(15 * 60) else { throw OTATransferError.invalidTimeout }
        guard !busy else { throw OTATransferError.busy }
        guard !opened, !closed else { throw OTATransferError.notReady }
        try Task.checkCancellation()
        opened = true; busy = true; defer { busy = false }
        let token = epoch
        var observed = expectedConnection
        do {
            let ready = try await pipe.readyConnection()
            if let expectedConnection, ready != expectedConnection { throw OTATransferError.generationChanged }
            observed = ready
            guard token == epoch, !closed else { throw OTATransferError.notReady }
            try Task.checkCancellation()
            guard ready.generation != 0, ready.maximumWriteBytes > 0 else { throw OTATransferError.notReady }
            connection = ready
        } catch {
            closed = true; epoch &+= 1; connection = nil
            // close() can run during readyConnection; its late result must not
            // publish a session and still needs its own generation disconnected.
            if let observed, observed.generation != 0 { await pipe.disconnect(generation: observed.generation) }
            throw error
        }
    }
    public func currentGeneration() async throws -> UInt64 {
        guard allowHostDevelopmentMutations else { throw OTATransferError.unqualified }
        guard !closed, let connection else { throw OTATransferError.notReady }
        let token = epoch
        do {
            let ready = try await pipe.readyConnection()
            try check(token)
            guard ready == connection else { throw OTATransferError.generationChanged }
            return connection.generation
        } catch {
            await close()
            throw error
        }
    }
    public func close() async {
        closed = true; epoch &+= 1
        let old = connection; connection = nil
        if let old { await pipe.disconnect(generation: old.generation) }
    }
    public func exchange(_ payload: Data, generation: UInt64,
                         deadline: ContinuousClock.Instant) async throws -> Data {
        guard allowHostDevelopmentMutations else { throw OTATransferError.unqualified }
        guard !busy else { throw OTATransferError.busy }
        guard !closed, let connection else { throw OTATransferError.notReady }
        busy = true; defer { busy = false }
        let token = epoch
        let started = ContinuousClock().now
        do {
            try check(token)
            guard generation == connection.generation else { throw OTATransferError.generationChanged }
            _ = try OTATransferCodec.decodeRequest(payload)
            guard nextID <= UInt64(UInt32.max) else { throw OTATransferError.outcomeUnknown }
            let id = UInt32(nextID); nextID += 1
            let packet = try GaugeFrameCodec.encode(.init(kind: .request, requestID: id, payload: payload))
            let fragmentBytes = min(connection.maximumWriteBytes, GaugeStreamReassembler.maximumChunkBytes)
            var offset = 0
            let pipe = self.pipe
            if let exclusive = pipe as? any GaugeRPCExchangePipe {
                try await Self.until(deadline) { try await exclusive.beginRequest(generation: generation) }
                try check(token)
            }
            while offset < packet.count {
                try await confirm(connection, token: token, deadline: deadline)
                let end = min(packet.count, offset + fragmentBytes)
                let bytes = packet.subdata(in: offset..<end)
                try await Self.until(deadline) { try await pipe.write(bytes, generation: generation) }
                try check(token)
                try await confirm(connection, token: token, deadline: deadline)
                offset = end
            }
            var decoder = GaugeStreamReassembler(frameTimeout: frameTimeout)
            while true {
                try await confirm(connection, token: token, deadline: deadline)
                try decoder.checkTimeout(at: ContinuousClock().now)
                let readDeadline = min(deadline, decoder.deadline ?? deadline)
                let chunk = try await Self.until(readDeadline) { try await pipe.receive(generation: generation) }
                try check(token)
                try await confirm(connection, token: token, deadline: deadline)
                let now = ContinuousClock().now
                guard chunk.receivedAt >= started, chunk.receivedAt <= now else { throw OTATransferError.invalidReply }
                let frames = try decoder.feed(chunk.data, at: chunk.receivedAt)
                try decoder.checkTimeout(at: ContinuousClock().now)
                if frames.isEmpty { continue }
                guard frames.count == 1, decoder.bufferedBytes == 0,
                      let response = frames.first, response.kind == .response, response.requestID == id else {
                    throw OTATransferError.invalidReply
                }
                do { _ = try OTATransferCodec.decodeResponse(response.payload) }
                catch { throw OTATransferError.invalidReply }
                if let exclusive = pipe as? any GaugeRPCExchangePipe {
                    try await Self.until(deadline) { try await exclusive.endRequest(generation: generation) }
                }
                try check(token)
                return response.payload
            }
        } catch {
            await close()
            throw error
        }
    }
    private func check(_ token: UInt64) throws {
        try Task.checkCancellation()
        guard token == epoch, !closed, connection != nil else { throw OTATransferError.notReady }
    }
    private func confirm(_ expected: GaugePipeConnection, token: UInt64,
                         deadline: ContinuousClock.Instant) async throws {
        try check(token)
        let pipe = self.pipe
        let ready = try await Self.until(deadline) { try await pipe.readyConnection() }
        try check(token)
        guard ready == expected else { throw OTATransferError.generationChanged }
    }
    /// Completion bounds rely on the pipe's cooperative cancellation contract.
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

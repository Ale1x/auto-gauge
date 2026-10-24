import Foundation

/// Bridges APP-04 endpoint packets into the same streaming RPC client used by the host C tests.
public actor InMemoryGaugePacketPipe: DevelopmentGaugePacketPipe {
    private let channel: any SimulatedPacketChannel
    private let clientID: String
    private var lease: SimulatedConnectionLease?
    private var decoder = GaugeStreamReassembler()
    private var queued: [GaugeIncomingChunk] = []
    private var queuedBytes = 0
    private var pending: UInt32?
    public init(channel: any SimulatedPacketChannel, clientID: String) { self.channel = channel; self.clientID = clientID }
    public func connect() async throws {
        if lease != nil { return }
        let opened = try await channel.connect(clientID: clientID)
        do { try Task.checkCancellation() } catch { await channel.disconnect(lease: opened); throw error }
        lease = opened; decoder.reset(); queued = []; queuedBytes = 0; pending = nil
    }
    public func readyConnection() async throws -> GaugePipeConnection {
        guard let lease else { throw GaugeLinkError.disconnected }
        return .init(generation: lease.token, maximumWriteBytes: 20)
    }
    public func write(_ data: Data, generation: UInt64) async throws {
        guard let lease, lease.token == generation, pending == nil, queued.isEmpty else { throw GaugeLinkError.notReady }
        let frames = try decoder.feed(data, at: ContinuousClock().now)
        guard frames.count <= 1 else { throw GaugeLinkError.invalidReply }
        if let frame = frames.first {
            guard frame.kind == .request else { throw GaugeLinkError.invalidReply }
            let batch = try await channel.begin(packet: GaugeFrameCodec.encode(frame), lease: lease)
            guard self.lease == lease else { throw GaugeLinkError.disconnected }
            try enqueue(batch.packets)
            pending = batch.hasContinuation ? frame.requestID : nil
        }
    }
    public func receive(generation: UInt64) async throws -> GaugeIncomingChunk {
        guard let lease, lease.token == generation else { throw GaugeLinkError.disconnected }
        try Task.checkCancellation()
        if queued.isEmpty, let id = pending {
            pending = nil
            let packets = try await channel.finish(requestID: id, lease: lease)
            guard self.lease == lease else { throw GaugeLinkError.disconnected }
            try enqueue(packets)
        }
        guard !queued.isEmpty else { throw GaugeLinkError.invalidReply }
        let chunk = queued.removeFirst(); queuedBytes -= chunk.data.count; return chunk
    }
    public func disconnect(generation: UInt64) async {
        guard let lease, lease.token == generation else { return }
        self.lease = nil; decoder.reset(); queued = []; queuedBytes = 0; pending = nil
        await channel.disconnect(lease: lease)
    }
    private func enqueue(_ packets: [Data]) throws {
        for packet in packets {
            _ = try GaugeFrameCodec.decode(packet)
            guard queuedBytes + packet.count <= 4112 else { throw GaugeLinkError.invalidChunk }
            for start in stride(from: 0, to: packet.count, by: 20) {
                let chunk = packet.subdata(in: start..<min(packet.count, start + 20))
                queued.append(.init(data: chunk)); queuedBytes += chunk.count
            }
        }
    }
}

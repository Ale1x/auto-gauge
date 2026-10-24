import Foundation
@testable import GaugeCore

actor OTAPacketMockPipe: GaugePacketPipe {
    enum Fault: Sendable, CaseIterable {
        case wrongID, event, extraFrame, trailingPrefix, crc, payload, futureTime, staleTime
        case generationAfterWrite, maximumAfterWrite, emptyChunk, stalledReceive, stalledWrite, slowWrite, slowReceive, partialStall
    }
    private var connection: GaugePipeConnection
    private var stream = GaugeStreamReassembler()
    private var incoming: [GaugeIncomingChunk] = []
    private let fault: Fault?
    private let responseFragmentBytes: Int
    private var readyPauseAt: Int?
    var writes: [Data] = []
    var requests: [GaugeFrame] = []
    var disconnects: [UInt64] = []
    var readyCalls = 0
    var writeEntered = false
    var receiveEntered = false
    private var receiveCalls = 0
    let staleTime = ContinuousClock().now.advanced(by: .seconds(-60))
    init(fault: Fault? = nil, maximumWriteBytes: Int = 17, responseFragmentBytes: Int = 3, generation: UInt64 = 51) {
        connection = .init(generation: generation, maximumWriteBytes: maximumWriteBytes)
        self.fault = fault; self.responseFragmentBytes = responseFragmentBytes
    }
    func pauseReady(at call: Int) { readyPauseAt = call }
    func readyConnection() async throws -> GaugePipeConnection {
        readyCalls += 1
        let observed = connection
        if readyCalls == readyPauseAt { try await Task.sleep(for: .milliseconds(100)) }
        return observed
    }
    func write(_ data: Data, generation: UInt64) async throws {
        guard generation == connection.generation else { throw GaugeLinkError.disconnected }
        writeEntered = true
        if fault == .stalledWrite { try await Task.sleep(for: .seconds(5)) }
        if fault == .slowWrite { try await Task.sleep(for: .milliseconds(100)) }
        writes.append(data)
        let frames = try stream.feed(data, at: ContinuousClock().now)
        for frame in frames {
            requests.append(frame)
            let req = try OTATransferCodec.decodeRequest(frame.payload)
            var body = try OTATransferCodec.encodeResponse(.init(status: .ok, phase: .manifest,
                transferID: req.transferID, manifestOffset: 0, imageOffset: 0))
            if fault == .payload { body.append(0) }
            let kind: GaugeFrameKind = fault == .event ? .event : .response
            let id: UInt32 = fault == .event ? 0 : (fault == .wrongID ? frame.requestID &+ 1 : frame.requestID)
            var packet = try GaugeFrameCodec.encode(.init(kind: kind, requestID: id, payload: body))
            if fault == .crc { packet[packet.count - 1] ^= 1 }
            if fault == .extraFrame { packet.append(packet) }
            if fault == .trailingPrefix { packet.append(0x41) }
            if fault == .generationAfterWrite { connection = .init(generation: generation + 1, maximumWriteBytes: connection.maximumWriteBytes) }
            if fault == .maximumAfterWrite { connection = .init(generation: generation, maximumWriteBytes: connection.maximumWriteBytes + 1) }
            let receivedAt = fault == .futureTime ? ContinuousClock().now.advanced(by: .seconds(60)) : (fault == .staleTime ? staleTime : ContinuousClock().now)
            // Extra/trailing attacks must reach the decoder in the same chunk.
            let fragmentBytes = fault == .extraFrame || fault == .trailingPrefix ? 512 : responseFragmentBytes
            var offset = 0
            while offset < packet.count {
                let end = min(packet.count, offset + fragmentBytes)
                incoming.append(.init(data: packet.subdata(in: offset..<end), receivedAt: receivedAt)); offset = end
            }
        }
    }
    func receive(generation: UInt64) async throws -> GaugeIncomingChunk {
        receiveEntered = true; receiveCalls += 1
        guard generation == connection.generation else { throw GaugeLinkError.disconnected }
        if fault == .partialStall && receiveCalls > 1 { try await Task.sleep(for: .seconds(5)) }
        if fault == .stalledReceive { try await Task.sleep(for: .seconds(5)) }
        if fault == .slowReceive { try await Task.sleep(for: .milliseconds(100)) }
        if fault == .emptyChunk { return .init(data: Data()) }
        guard !incoming.isEmpty else { throw GaugeLinkError.disconnected }
        return incoming.removeFirst()
    }
    func disconnect(generation: UInt64) async { disconnects.append(generation) }
}

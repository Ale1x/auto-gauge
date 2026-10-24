import Foundation

public protocol GaugePacketPipe: Sendable {
    /// Must fail until indication CCCD is confirmed. Never starts a scan or pairing implicitly.
    func readyConnection() async throws -> GaugePipeConnection
    /// Completes only after Write With Response callback; ATT success is not RPC applied.
    func write(_ data: Data, generation: UInt64) async throws
    /// At most one waiter; implementation must unblock on cancellation/disconnect.
    func receive(generation: UInt64) async throws -> GaugeIncomingChunk
    func disconnect(generation: UInt64) async
}

import Foundation
import GaugeCore

/// Only the qualified session owns this pipe. No qualification is established by
/// constructing it; the core provider must authorize before beginRequest writes.
struct NativeQualifiedPacketPipe: GaugeRPCExchangePipe {
    let controller: NativeBluetoothController
    let generation: UInt64
    func readyConnection() async throws -> GaugePipeConnection {
        let ready = try await controller.readyConnection()
        guard ready.generation == generation else { throw GaugeLinkError.disconnected }; return ready
    }
    func beginRequest(generation: UInt64) async throws {
        guard generation == self.generation else { throw GaugeLinkError.disconnected }
        try await controller.beginQualifiedRequest(generation: generation)
    }
    func endRequest(generation: UInt64) async throws {
        guard generation == self.generation else { throw GaugeLinkError.disconnected }
        try await controller.endQualifiedRequest(generation: generation)
    }
    func write(_ data: Data, generation: UInt64) async throws {
        guard generation == self.generation else { throw GaugeLinkError.disconnected }
        try await controller.write(data, generation: generation)
    }
    func receive(generation: UInt64) async throws -> GaugeIncomingChunk {
        guard generation == self.generation else { throw GaugeLinkError.disconnected }
        return try await controller.receive(generation: generation)
    }
    func disconnect(generation: UInt64) async { await controller.disconnect(generation: generation) }
}

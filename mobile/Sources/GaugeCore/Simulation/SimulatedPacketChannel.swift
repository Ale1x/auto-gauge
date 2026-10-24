import Foundation

public protocol SimulatedPacketChannel: Sendable {
    func connect(clientID: String) async throws -> SimulatedConnectionLease
    func disconnect(lease: SimulatedConnectionLease) async
    func begin(packet: Data, lease: SimulatedConnectionLease) async throws -> SimulatedReplyBatch
    func finish(requestID: UInt32, lease: SimulatedConnectionLease) async throws -> [Data]
}

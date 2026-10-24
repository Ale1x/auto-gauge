import Foundation

/// A single externally managed, trusted session. This protocol supplies no BLE authorization.
/// Implementations must honor deadline/cancellation and return one matched complete AG response
/// payload, rejecting extra/stale frames. Generation must change whenever the session changes.
public protocol OTATransferTransport: Sendable {
    func currentGeneration() async throws -> UInt64
    func exchange(_ payload: Data, generation: UInt64, deadline: ContinuousClock.Instant) async throws -> Data
}

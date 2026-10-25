import Foundation

/// Implemented by the qualified local platform integration. It must acquire an
/// exclusive OTA route, validate target identity/hardware/current trust policy,
/// and return a fresh lease. It must not share a live RPC receive stream.
/// After an interrupted prior installation it must reconcile target state before
/// granting another session; constructing a new controller is not reconciliation.
/// Opening must honor the operation deadline and cooperative cancellation.
public protocol OTAInstallationSessionProvider: Sendable {
    func openSession(for candidate: VerifiedOTACandidate, deadline: ContinuousClock.Instant) async throws -> any OTAInstallationSession
}

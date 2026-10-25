import Foundation

/// Full configuration/history session used by app dependency injection. The
/// transport's locally established source is supplied separately by the binding.
public protocol GaugeSessionTransport: GaugeTransport {
    func connect() async throws
    func disconnect() async
    func isConnected() async -> Bool
    func requiresRead() async -> Bool
    func commitState() async -> GaugeCommitState
    func deviceIdentifier() async throws -> String
    func lastPersistence() async -> String?
    func validateDraft(_ draft: GaugeConfiguration) async throws
    func applyDraft(_ draft: GaugeConfiguration, onPhase: @Sendable (GaugeCommitState) async -> Void) async throws -> GaugeConfiguration
    func importHistoryPage(sessionID: LogSessionID, archive: LocalLogArchive, limit: Int) async throws -> GaugeLogImportResult
}

extension DevelopmentGaugeTransport: GaugeSessionTransport {}

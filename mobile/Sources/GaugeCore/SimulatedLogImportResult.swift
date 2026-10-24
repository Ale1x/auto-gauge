import Foundation

public struct SimulatedLogImportResult: Sendable {
    public let acknowledgement: HistoricalLogAcknowledgement
    public let peerAcknowledged: Bool
    public let warning: String?
}

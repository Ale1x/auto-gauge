import Foundation

public struct GaugeLogImportResult: Sendable {
    public let acknowledgement: HistoricalLogAcknowledgement
    public let peerAcknowledged: Bool
    public let warning: String?
}

import Foundation

public struct GaugeLogImportResult: Sendable {
    public let acknowledgement: HistoricalLogAcknowledgement
    public let peerAcknowledged: Bool
    public let warning: String?
    public let qualifiedImport: QualifiedHistoryImport?
    init(acknowledgement: HistoricalLogAcknowledgement, peerAcknowledged: Bool, warning: String?, qualifiedImport: QualifiedHistoryImport? = nil) {
        self.acknowledgement = acknowledgement; self.peerAcknowledged = peerAcknowledged
        self.warning = warning; self.qualifiedImport = qualifiedImport
    }
}

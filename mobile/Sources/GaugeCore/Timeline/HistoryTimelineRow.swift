import Foundation

public struct HistoryTimelineRow: Equatable, Identifiable, Sendable {
    public let record: HistoricalLogRecord
    public let value: Double?
    public let quality: HistorySampleQuality
    public var id: String { record.sequence.decimal }
}

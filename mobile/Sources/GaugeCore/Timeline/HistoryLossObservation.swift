import Foundation

public struct HistoryLossObservation: Equatable, Identifiable, Sendable {
    public let interval: LogLossInterval
    public let availableRecords: Int
    public var id: String { interval.id }
    public var unavailableCount: UInt64 { interval.last.value - interval.first.value + 1 - UInt64(availableRecords) }
}

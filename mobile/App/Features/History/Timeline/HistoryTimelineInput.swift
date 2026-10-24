import GaugeCore

enum HistoryTimelineInput: Equatable, Sendable {
    case gauge([ImportedLogSession], ready: Bool)
    case cloud([CloudHistoryItem])
}

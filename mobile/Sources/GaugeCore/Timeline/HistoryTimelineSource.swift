public enum HistoryTimelineSource: String, Hashable, Sendable {
    case simulatedGauge, restoredCloud
    public var titleIT: String { self == .simulatedGauge ? "Archivio del gauge simulato" : "Copia locale recuperata da iCloud" }
}

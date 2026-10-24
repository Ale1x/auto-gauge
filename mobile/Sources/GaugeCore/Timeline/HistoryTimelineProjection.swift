import Foundation

public struct HistoryTimelineProjection: Equatable, Sendable {
    public let rows: [HistoryTimelineRow]
    public let chart: [HistoryChartPoint]
    public let numericCount: Int
    public let drawLines: Bool
    public let originMS: LogUInt64?
    public let timePrecisionLimited: Bool
    public let qualityCounts: [HistoryQualityCount]
    public let lossObservations: [HistoryLossObservation]
    public let copyGaps: [LogLossInterval]
    public let clockRegressions: Int
    public var decimated: Bool { chart.count < numericCount }
}

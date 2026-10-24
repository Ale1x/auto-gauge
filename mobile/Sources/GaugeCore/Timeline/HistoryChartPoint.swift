import Foundation

public struct HistoryChartPoint: Equatable, Identifiable, Sendable {
    public let sequence: LogUInt64
    public let monotonicMS: LogUInt64
    public let offsetMS: UInt64
    public let value: Double
    public let segment: Int
    public var id: String { sequence.decimal }
    public var seconds: Double { Double(offsetMS / 1000) + Double(offsetMS % 1000) / 1000 }
}

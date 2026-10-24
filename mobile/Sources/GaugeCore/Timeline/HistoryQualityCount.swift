public struct HistoryQualityCount: Equatable, Identifiable, Sendable {
    public let status: LogRecordStatus
    public let count: Int
    public var id: String { status.rawValue }
}

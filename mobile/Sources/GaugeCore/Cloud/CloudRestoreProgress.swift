import Foundation

public struct CloudRestoreProgress: Equatable, Sendable {
    public let items: [CloudHistoryItem]
    public let scanFinished: Bool
    public let hasCheckpoint: Bool
    public var recordCount: Int { items.filter { $0.record != nil }.count }
    public var observedLossCount: Int { items.filter { $0.loss != nil }.count }
}

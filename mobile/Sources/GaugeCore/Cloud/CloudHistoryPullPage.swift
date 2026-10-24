import Foundation

public struct CloudHistoryPullPage: Sendable {
    public static let maximumRecords = 16
    public static let maximumBytes = 64 * 1024
    public static let maximumCursorBytes = 64 * 1024
    public let records: [CloudHistoryRemoteRecord]
    public let nextCursor: Data?
    public init(records: [CloudHistoryRemoteRecord], nextCursor: Data?) throws {
        guard records.count <= Self.maximumRecords,
              records.allSatisfy({ $0.payload.count <= 4096 && $0.recordID.utf8.count <= 128 && $0.digest.utf8.count <= 64 }),
              records.reduce(0, { $0 + $1.payload.count + $1.recordID.utf8.count + $1.digest.utf8.count }) <= Self.maximumBytes,
              nextCursor.map({ !$0.isEmpty && $0.count <= Self.maximumCursorBytes }) ?? true else { throw CloudSyncError.capacity }
        self.records = records; self.nextCursor = nextCursor
    }
}

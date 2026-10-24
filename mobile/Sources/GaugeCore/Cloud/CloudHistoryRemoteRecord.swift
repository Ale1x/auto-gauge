import Foundation

public struct CloudHistoryRemoteRecord: Sendable {
    public let recordID: String
    public let payload: Data
    public let digest: String
    public init(recordID: String, payload: Data, digest: String) {
        self.recordID = recordID; self.payload = payload; self.digest = digest
    }
    public init(item: CloudHistoryItem) throws {
        self.recordID = item.recordID; payload = try item.payload(); digest = CloudHistoryItem.digest(payload)
    }
    public func validatedItem() throws -> CloudHistoryItem {
        guard payload.count <= 4096, recordID.utf8.count <= 128, digest.utf8.count == 64,
              digest == CloudHistoryItem.digest(payload) else { throw CloudSyncError.invalidData }
        let item = try CloudHistoryItem.decode(payload)
        guard item.recordID == recordID else { throw CloudSyncError.conflict }
        return item
    }
}

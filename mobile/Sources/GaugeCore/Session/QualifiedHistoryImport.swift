/// Receipt created only by the qualified native session after local archive commit.
/// No public initializer/Codable can turn a simulator import into this receipt.
public struct QualifiedHistoryImport: Sendable {
    public let deviceID: String
    public let sessionID: LogSessionID
    public let items: [CloudHistoryItem]
    /// Recovered only from an archive's atomically committed qualified intent.
    init(recovering recovered: [CloudHistoryItem]) throws {
        guard let first = recovered.first else { throw CloudSyncError.invalidData }
        for item in recovered {
            _ = try CloudHistoryItem.decode(item.payload())
            guard item.deviceID == first.deviceID, item.sessionID == first.sessionID else { throw CloudSyncError.invalidData }
        }
        deviceID = first.deviceID; sessionID = first.sessionID; items = recovered
    }
    init(deviceID: String, page: HistoricalLogPage) throws {
        self.deviceID = deviceID; sessionID = page.sessionID
        items = try page.records.map { try CloudHistoryItem(deviceID: deviceID, sessionID: page.sessionID, record: $0) }
            + page.lost.map { try CloudHistoryItem(deviceID: deviceID, sessionID: page.sessionID, loss: $0) }
    }
}

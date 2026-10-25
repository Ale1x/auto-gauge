/// Receipt created only by the qualified native session after local archive commit.
/// No public initializer/Codable can turn a simulator import into this receipt.
public struct QualifiedHistoryImport: Sendable {
    public let deviceID: String
    public let sessionID: LogSessionID
    public let items: [CloudHistoryItem]
    init(deviceID: String, page: HistoricalLogPage) throws {
        self.deviceID = deviceID; sessionID = page.sessionID
        items = try page.records.map { try CloudHistoryItem(deviceID: deviceID, sessionID: page.sessionID, record: $0) }
            + page.lost.map { try CloudHistoryItem(deviceID: deviceID, sessionID: page.sessionID, loss: $0) }
    }
}

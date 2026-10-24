import Foundation

public struct GaugeRPCResponse: Codable, Sendable {
    public let op: String?
    public let deviceID: String?
    public let phase: String?
    public let error: String?
    public let protocolVersion: Int?
    public let supportedService01PIDs: [Int]?
    public let appliedRevision: Int?
    public let persistence: String?
    public let storeState: GaugeEndpointStoreState?
    public let simulation: Bool?
    public let authenticated: Bool?
    public internal(set) var configuration: GaugeConfiguration?
    public let page: HistoricalLogPage?
    public let sessionID: String?
    public let acknowledgedThrough: LogUInt64?
    public let sourceRecordsDeleted: Bool?
    enum CodingKeys: String, CodingKey {
        case op, phase, error, persistence, simulation, authenticated, configuration, page
        case storeState = "store_state", deviceID = "device_id", supportedService01PIDs = "supported_service01_pids", protocolVersion = "protocol_version", appliedRevision = "applied_revision", sessionID = "session_id", acknowledgedThrough = "acknowledged_through", sourceRecordsDeleted = "source_records_deleted"
    }
}

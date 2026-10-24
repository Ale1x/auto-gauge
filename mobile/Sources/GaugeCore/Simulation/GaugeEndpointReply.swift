import Foundation

struct GaugeEndpointReply: Codable {
    var simulation = true
    var op: String? = nil
    var phase: String? = nil
    var error: String? = nil
    var deviceID: String? = nil
    var protocolVersion: Int? = nil
    var supportedService01PIDs: [Int]? = nil
    var authenticated: Bool? = nil
    var configuration: GaugeConfiguration? = nil
    var persistence: String? = nil
    var appliedRevision: Int? = nil
    var page: HistoricalLogPage? = nil
    var sessionID: String? = nil
    var acknowledgedThrough: LogUInt64? = nil
    var sourceRecordsDeleted: Bool? = nil
    enum CodingKeys: String, CodingKey {
        case simulation, op, phase, error, authenticated, configuration, persistence
        case deviceID = "device_id", protocolVersion = "protocol_version", supportedService01PIDs = "supported_service01_pids", appliedRevision = "applied_revision", page, sessionID = "session_id", acknowledgedThrough = "acknowledged_through", sourceRecordsDeleted = "source_records_deleted"
    }
}

/// Volatile, explicitly simulated device. No radio, owner registry or real authorization.

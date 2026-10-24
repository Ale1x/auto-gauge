import Foundation

public struct GaugeGATTProfile: Sendable {
    public let serviceUUID: UUID
    public let requestUUID: UUID
    public let responseUUID: UUID
    public static func development() throws -> Self {
        guard let url = Bundle.module.url(forResource: "gatt.profile", withExtension: "json") else { throw GaugeLinkError.notReady }
        return try decode(Data(contentsOf: url))
    }
    public static func decode(_ data: Data) throws -> Self {
        guard data.count <= 4096 else { throw GaugeLinkError.invalidReply }
        try StrictLogJSON.check(data)
        let wire = try JSONDecoder().decode(GaugeGATTProfileWire.self, from: data)
        guard wire.profile_version == 1,
              wire.status == "DEVELOPMENT_CANDIDATE_NOT_RADIO_VALIDATED",
              wire.request_write == "withResponse", wire.response_delivery == "indication",
              wire.maximum_active_connections == 1, wire.maximum_pending_requests == 1,
              wire.maximum_chunk_bytes == 512, wire.maximum_payload_bytes == 4096, wire.maximum_frame_bytes == 4112,
              wire.frame_deadline_ms == 10000, wire.request_deadline_ms == 30000, wire.scan_deadline_ms == 10000,
              wire.security == "LESC_MITM_128BIT_ENCRYPTED_WITH_LOCAL_NEW_PEER_ADMISSION",
              Set([wire.service_uuid, wire.request_uuid, wire.response_uuid]).count == 3 else { throw GaugeLinkError.invalidReply }
        return Self(serviceUUID: wire.service_uuid, requestUUID: wire.request_uuid, responseUUID: wire.response_uuid)
    }
}

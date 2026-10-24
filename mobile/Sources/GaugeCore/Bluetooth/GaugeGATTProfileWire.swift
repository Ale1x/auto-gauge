import Foundation

struct GaugeGATTProfileWire: Decodable {
    let profile_version: Int
    let status: String
    let service_uuid: UUID
    let request_uuid: UUID
    let response_uuid: UUID
    let request_write: String
    let response_delivery: String
    let maximum_active_connections: Int
    let maximum_pending_requests: Int
    let maximum_chunk_bytes: Int
    let maximum_payload_bytes: Int
    let maximum_frame_bytes: Int
    let frame_deadline_ms: Int
    let request_deadline_ms: Int
    let scan_deadline_ms: Int
    let security: String
}

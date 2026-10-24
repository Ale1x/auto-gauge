#if os(macOS)
import Foundation

struct OTAVectorCase: Decodable, Sendable {
    let name: String
    let envelope_base64: String
    let expected_package: Bool
    let canonical_base64: String
    let hardware_id: String?
    let inactive_capacity: Int?
    let accepted_counter: String?
    let key_id: String?
    let key_der_base64: String?
    let image_base64: String?
}
#endif

import Foundation

/// Trusted local provisioning, never decoded from an imported package or archive.
/// The caller must refresh capacity/floor from trustworthy device policy before use.
public struct OTAPreparationTrust: Sendable {
    public let hardwareID: String
    public let inactiveCapacity: Int
    public let acceptedCounter: UInt64
    public let trustedKeys: [String: Data]
    public init(hardwareID: String, inactiveCapacity: Int, acceptedCounter: UInt64, trustedKeys: [String: Data]) {
        self.hardwareID = hardwareID; self.inactiveCapacity = inactiveCapacity; self.acceptedCounter = acceptedCounter
        self.trustedKeys = trustedKeys.mapValues { value in
            value.withUnsafeBytes { bytes in
                guard let base = bytes.baseAddress else { return Data() }
                return Data(bytes: base, count: bytes.count)
            }
        }
    }
}

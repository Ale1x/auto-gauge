import Foundation

public struct OTAManifest: Equatable, Sendable {
    public let hardwareID: String
    public let firmwareVersion: String
    public let releaseCounter: UInt64
    public let imageBytes: Int
    public let imageSHA256: String
    public let signingKeyID: String
    init(json:OTAStrictJSON) throws {
        let keys: Set<String> = ["schema_version","target","hardware_id","firmware_version","release_counter","image_bytes","image_sha256","signing_key_id"]
        guard let fields = json.object, Set(fields.keys) == keys, fields["schema_version"]?.integer == 1,
              fields["target"]?.string == "esp32s3" else { throw OTAPackageError.schema }
        guard let hardware = fields["hardware_id"]?.string, Self.identifier(hardware,version:false),
              let key = fields["signing_key_id"]?.string, Self.identifier(key,version:false),
              let version = fields["firmware_version"]?.string, Self.identifier(version,version:true),
              let counter = fields["release_counter"]?.string, let release = UInt64(counter), String(release) == counter,
              let size = fields["image_bytes"]?.integer, (1...OTAPackageVerifier.maximumImageBytes).contains(size),
              let hash = fields["image_sha256"]?.string, hash.utf8.count == 64,
              hash.utf8.allSatisfy({ (48...57).contains($0) || (97...102).contains($0) }) else { throw OTAPackageError.manifest }
        hardwareID = hardware; signingKeyID = key; firmwareVersion = version
        releaseCounter = release; imageBytes = size; imageSHA256 = hash
    }
    /// ESP target verifier additionally bounds version storage and image alignment.
    /// Generic reference JSON/signing bytes remain schema-compatible (version≤64).
    func validateESPProfile() throws {
        guard firmwareVersion.utf8.count <= 31, imageBytes.isMultiple(of:16) else { throw OTAPackageError.profile }
    }
    private static func identifier(_ string:String,version:Bool) -> Bool {
        let bytes = Array(string.utf8)
        func alphanumeric(_ byte:UInt8) -> Bool { (48...57).contains(byte) || (65...90).contains(byte) || (97...122).contains(byte) }
        guard (1...64).contains(bytes.count), let first = bytes.first, alphanumeric(first) else { return false }
        return bytes.allSatisfy { alphanumeric($0) || [46,95,45].contains($0) || (version && $0 == 43) }
    }
    /// Fields are validated ASCII without JSON metacharacters; fixed order is Python sort_keys=True.
    var signingBytes: Data {
        let body = "{\"firmware_version\":\"\(firmwareVersion)\",\"hardware_id\":\"\(hardwareID)\",\"image_bytes\":\(imageBytes),\"image_sha256\":\"\(imageSHA256)\",\"release_counter\":\"\(releaseCounter)\",\"schema_version\":1,\"signing_key_id\":\"\(signingKeyID)\",\"target\":\"esp32s3\"}"
        return Data("AUTO-GAUGE-OTA-MANIFEST\0V1\0".utf8) + Data(body.utf8)
    }
}

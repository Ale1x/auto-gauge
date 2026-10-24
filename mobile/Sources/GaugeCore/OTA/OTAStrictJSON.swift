import Foundation

/// Restricted to the object/string/integer grammar used by OTA schema1.
/// Decodes keys before duplicate detection; neither JSONDecoder nor NSDictionary
/// can prove that an input object did not contain duplicate keys.
indirect enum OTAStrictJSON {
    case object([String: Self]), string(String), integer(String)
    static func parse(_ data: Data) throws -> Self {
        guard (1...OTAPackageVerifier.maximumEnvelopeBytes).contains(data.count) else { throw OTAPackageError.envelope }
        var parser = OTAStrictJSONParser(bytes:Array(data))
        let result = try parser.value(depth:0)
        parser.whitespace()
        guard parser.index == parser.bytes.count else { throw OTAPackageError.envelope }
        return result
    }
    var object: [String:Self]? { if case .object(let result) = self { result } else { nil } }
    var string: String? { if case .string(let result) = self { result } else { nil } }
    var integer: Int? { if case .integer(let result) = self { Int(result) } else { nil } }
}

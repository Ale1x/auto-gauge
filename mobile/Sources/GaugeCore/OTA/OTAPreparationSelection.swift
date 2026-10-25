public struct OTAPreparationSelection: Equatable, Sendable {
    public let name: String
    public let byteCount: Int
    public init(name: String, byteCount: Int) { self.name = name; self.byteCount = byteCount }
}

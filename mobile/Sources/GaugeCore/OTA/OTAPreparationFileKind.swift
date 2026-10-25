public enum OTAPreparationFileKind: Equatable, Sendable {
    case manifest, image
    public var maximumBytes: Int {
        switch self {
        case .manifest: OTAPackageVerifier.maximumEnvelopeBytes
        case .image: OTAPackageVerifier.maximumImageBytes
        }
    }
}

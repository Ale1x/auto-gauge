public struct OTAPreparationSnapshot: Equatable, Sendable {
    public let manifestFile: OTAPreparationSelection?
    public let imageFile: OTAPreparationSelection?
    public let verifiedManifest: OTAManifest?
    public init(manifestFile: OTAPreparationSelection? = nil, imageFile: OTAPreparationSelection? = nil, verifiedManifest: OTAManifest? = nil) {
        self.manifestFile = manifestFile; self.imageFile = imageFile; self.verifiedManifest = verifiedManifest
    }
}

public struct OTAInstallationProgress: Equatable, Sendable {
    public let manifestAcknowledged: UInt32
    public let imageAcknowledged: UInt32
    public init(manifestAcknowledged: UInt32 = 0, imageAcknowledged: UInt32 = 0) {
        self.manifestAcknowledged = manifestAcknowledged; self.imageAcknowledged = imageAcknowledged
    }
}

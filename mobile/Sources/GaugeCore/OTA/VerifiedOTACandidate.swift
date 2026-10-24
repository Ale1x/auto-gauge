import Foundation

/// Verified immutable byte snapshot only: not installed, flashed, bootable or applied.
/// No public initializer or Codable conformance can manufacture a verified result.
public struct VerifiedOTACandidate: Sendable {
    public let manifest: OTAManifest
    public let envelopeSnapshot: Data
    public let imageSnapshot: Data
    init(manifest:OTAManifest,envelope:Data,image:Data) {
        self.manifest = manifest; envelopeSnapshot = envelope; imageSnapshot = image
    }
}

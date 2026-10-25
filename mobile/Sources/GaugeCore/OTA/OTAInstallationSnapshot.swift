public struct OTAInstallationSnapshot: Equatable, Sendable {
    public let manifest: OTAManifest?
    public let state: OTAInstallationState
    public let progress: OTAInstallationProgress
    public let selectionMayHaveChanged: Bool
    public init(manifest: OTAManifest? = nil, state: OTAInstallationState = .idle, progress: OTAInstallationProgress = .init(), selectionMayHaveChanged: Bool = false) {
        self.manifest = manifest; self.state = state; self.progress = progress; self.selectionMayHaveChanged = selectionMayHaveChanged
    }
}

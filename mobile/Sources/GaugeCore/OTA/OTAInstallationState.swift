public enum OTAInstallationState: Equatable, Sendable {
    case idle, opening, uploading, ready, selecting, selected, unknown
}

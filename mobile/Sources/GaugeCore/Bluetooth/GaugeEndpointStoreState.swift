public enum GaugeEndpointStoreState: String, Codable, Sendable {
    case readOnly = "read_only"
    case capabilityMismatch = "capability_mismatch"
    case noValidConfiguration = "no_valid_configuration"
    case conflict
    case ioOrIntegrityError = "io_or_integrity_error"
    case commitOrReadbackFailed = "commit_or_readback_failed"
}

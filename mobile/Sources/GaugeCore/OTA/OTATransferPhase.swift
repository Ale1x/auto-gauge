public enum OTATransferPhase: UInt8, Sendable {
    case idle = 0, manifest, writing, ready, selected, fault
}

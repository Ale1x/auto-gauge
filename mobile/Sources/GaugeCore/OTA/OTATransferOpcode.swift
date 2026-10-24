public enum OTATransferOpcode: UInt8, Sendable {
    case start = 1, manifest, begin, image, finish, select, cancel
}

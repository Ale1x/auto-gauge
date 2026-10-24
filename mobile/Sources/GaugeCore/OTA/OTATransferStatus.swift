public enum OTATransferStatus: UInt8, Sendable {
    case ok = 0, invalid, denied, expired, backendFailure, busy
}

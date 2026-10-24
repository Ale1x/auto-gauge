public enum OTATransferError: Error, Equatable, Sendable {
    case invalidPayload, unqualified, busy, notReady, invalidTimeout, timeout, generationChanged
    case outcomeUnknown, invalidReply, rejected(OTATransferStatus)
}

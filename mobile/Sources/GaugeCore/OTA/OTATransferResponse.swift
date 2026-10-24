public struct OTATransferResponse: Sendable, Equatable {
    public let status: OTATransferStatus
    public let phase: OTATransferPhase
    public let selectionMayHaveChanged: Bool
    public let transferID: UInt32
    public let manifestOffset: UInt32
    public let imageOffset: UInt32
    public init(status: OTATransferStatus, phase: OTATransferPhase, selectionMayHaveChanged: Bool = false,
                transferID: UInt32, manifestOffset: UInt32, imageOffset: UInt32) {
        self.status = status; self.phase = phase; self.selectionMayHaveChanged = selectionMayHaveChanged
        self.transferID = transferID; self.manifestOffset = manifestOffset; self.imageOffset = imageOffset
    }
}

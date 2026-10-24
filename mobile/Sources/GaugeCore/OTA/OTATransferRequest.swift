import Foundation

public struct OTATransferRequest: Sendable, Equatable {
    public let opcode: OTATransferOpcode
    public let transferID: UInt32
    public let offset: UInt32
    public let body: Data
    public init(opcode: OTATransferOpcode, transferID: UInt32, offset: UInt32 = 0, body: Data = Data()) {
        self.opcode = opcode; self.transferID = transferID; self.offset = offset; self.body = body
    }
}

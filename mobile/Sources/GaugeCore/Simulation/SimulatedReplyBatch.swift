import Foundation

public struct SimulatedReplyBatch: Sendable {
    public let packets: [Data]
    public let hasContinuation: Bool
    public init(packets: [Data], hasContinuation: Bool) { self.packets = packets; self.hasContinuation = hasContinuation }
}

/// This API schedules in-memory delivery; continuation is not an extra wire command.

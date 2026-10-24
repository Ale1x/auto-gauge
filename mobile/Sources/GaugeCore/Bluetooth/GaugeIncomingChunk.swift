import Foundation

public struct GaugeIncomingChunk: Sendable {
    public let data: Data
    public let receivedAt: ContinuousClock.Instant
    public init(data: Data, receivedAt: ContinuousClock.Instant = ContinuousClock().now) {
        self.data = data; self.receivedAt = receivedAt
    }
}

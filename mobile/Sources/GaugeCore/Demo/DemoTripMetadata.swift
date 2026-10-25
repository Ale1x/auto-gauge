import Foundation

public struct DemoTripMetadata: Sendable {
    public let sessionID: LogSessionID
    public let title: String
    public let date: Date
    public let durationSeconds: Int
}

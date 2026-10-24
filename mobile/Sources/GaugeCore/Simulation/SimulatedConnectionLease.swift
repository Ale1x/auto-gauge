import Foundation

public struct SimulatedConnectionLease: Equatable, Sendable {
    public let clientID: String
    public let token: UInt64
}

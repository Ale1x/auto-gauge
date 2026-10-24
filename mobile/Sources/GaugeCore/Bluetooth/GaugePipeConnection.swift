import Foundation

public struct GaugePipeConnection: Equatable, Sendable {
    public let generation: UInt64
    public let maximumWriteBytes: Int
    public init(generation: UInt64, maximumWriteBytes: Int) {
        self.generation = generation; self.maximumWriteBytes = maximumWriteBytes
    }
}

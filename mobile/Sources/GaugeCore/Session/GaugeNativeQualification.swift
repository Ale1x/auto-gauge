import Foundation

/// Local qualification decision bound to one observed pipe generation/device.
/// Only a platform qualification provider may establish this context.
public struct GaugeNativeQualification: Equatable, Sendable {
    public let generation: UInt64
    public let deviceID: String
    public init(generation: UInt64, deviceID: String) throws {
        guard generation != 0, !deviceID.isEmpty, deviceID.utf8.count <= 128,
              deviceID.unicodeScalars.allSatisfy({ (33...126).contains($0.value) }) else { throw GaugeLinkError.unqualified }
        self.generation = generation; self.deviceID = deviceID
    }
}

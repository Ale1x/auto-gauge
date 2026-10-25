import Foundation
@testable import GaugeCore

/// Explicit trusted LOCAL test decision, not Bluetooth/authentication evidence.
actor QualifiedSessionTestProvider: GaugeNativeQualificationProvider {
    var allowed = true
    var calls = 0
    let deviceID: String
    init(deviceID: String = "QUALIFIED-HOST-TEST") { self.deviceID = deviceID }
    func setAllowed(_ value: Bool) { allowed = value }
    func qualification(for connection: GaugePipeConnection) async throws -> GaugeNativeQualification {
        calls += 1
        guard allowed else { throw GaugeLinkError.unqualified }
        return try .init(generation: connection.generation, deviceID: deviceID)
    }
}

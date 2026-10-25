/// Production default until actual platform/server security is qualified.
public struct UnqualifiedNativeGaugeProvider: GaugeNativeQualificationProvider {
    public init() {}
    public func qualification(for connection: GaugePipeConnection) async throws -> GaugeNativeQualification {
        throw GaugeLinkError.unqualified
    }
}

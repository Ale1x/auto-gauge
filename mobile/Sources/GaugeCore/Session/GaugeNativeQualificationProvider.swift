public protocol GaugeNativeQualificationProvider: Sendable {
    /// Must establish current trusted local authorization and server/transport
    /// qualification, not infer it from remote authenticated/simulation fields.
    func qualification(for connection: GaugePipeConnection) async throws -> GaugeNativeQualification
}

actor GaugeBoundSessionAuthorization: GaugeRPCSessionAuthorization {
    private let provider: any GaugeNativeQualificationProvider
    private var bound: GaugeNativeQualification?
    init(provider: any GaugeNativeQualificationProvider) { self.provider = provider }
    func bind(_ connection: GaugePipeConnection) async throws -> GaugeNativeQualification {
        let grant = try await provider.qualification(for: connection)
        guard grant.generation == connection.generation else { throw GaugeLinkError.unqualified }
        bound = grant; return grant
    }
    func invalidate() { bound = nil }
    func authorize(_ connection: GaugePipeConnection) async throws {
        guard let expected = bound, expected.generation == connection.generation else { throw GaugeLinkError.unqualified }
        let current = try await provider.qualification(for: connection)
        guard bound == expected, current == expected else { throw GaugeLinkError.unqualified }
    }
}

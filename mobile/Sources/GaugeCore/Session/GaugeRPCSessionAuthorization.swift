/// Trusted local authorization, checked before sending and before consuming each
/// response frame. Conformance is a platform contract, never hello proof.
public protocol GaugeRPCSessionAuthorization: Sendable {
    func authorize(_ connection: GaugePipeConnection) async throws
}

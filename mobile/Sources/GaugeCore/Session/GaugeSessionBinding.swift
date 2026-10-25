public struct GaugeSessionBinding: Sendable {
    public let transport: any GaugeSessionTransport
    public let isNative: Bool
    private let catalog: @Sendable () async throws -> [LogSessionID]
    private init(transport: any GaugeSessionTransport, isNative: Bool, catalog: @escaping @Sendable () async throws -> [LogSessionID]) {
        self.transport = transport; self.isNative = isNative; self.catalog = catalog
    }
    public static func simulator(_ transport: DevelopmentGaugeTransport, sessions: [LogSessionID]) -> Self {
        .init(transport: transport, isNative: false, catalog: { sessions })
    }
    public static func qualifiedNative(_ transport: QualifiedGaugeTransport) -> Self {
        .init(transport: transport, isNative: true, catalog: { try await transport.historySessionIDs() })
    }
    public func historySessionIDs() async throws -> [LogSessionID] { try await catalog() }
}

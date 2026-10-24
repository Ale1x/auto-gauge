public enum GaugeRPCPersistencePolicy: Sendable {
    /// Existing in-memory development contract; never implies durable device storage.
    case volatileDevelopment
    /// Host C integration explicitly configured against a verified file store. Not radio/flash proof.
    case hostDurableStore
    func accepts(_ label: String?) -> Bool {
        switch self {
        case .volatileDevelopment: return label == "volatile"
        case .hostDurableStore: return label == "durable"
        }
    }
}

public enum HistorySampleQuality: Equatable, Sendable {
    case decoded, sourceStatus(LogRecordStatus), formulaUnavailable, wrongPayloadLength, clockRegression
    public var titleIT: String {
        switch self {
        case .decoded: return "Valore decodificato"
        case .sourceStatus(let status): return status.titleIT
        case .formulaUnavailable: return "Formula non qualificata in questa versione · solo raw"
        case .wrongPayloadLength: return "Lunghezza raw incoerente con la formula"
        case .clockRegression: return "Tempo monotono incoerente · escluso dal grafico"
        }
    }
}

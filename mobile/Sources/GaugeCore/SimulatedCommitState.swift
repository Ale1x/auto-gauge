import Foundation

public enum SimulatedCommitState: Equatable, Sendable {
    case idle, received, appliedVolatile(Int), rejected(String), unknown
    public var titleIT: String {
        switch self {
        case .idle: return "Nessuna applicazione in corso"
        case .received: return "Ricevuta dal simulatore · non ancora applicata"
        case .appliedVolatile(let revision): return "Applicata in RAM simulata · revisione \(revision) · volatile"
        case .rejected(let reason): return "Rifiutata dal simulatore: \(reason)"
        case .unknown: return "Esito sconosciuto · rileggi il gauge simulato"
        }
    }
}

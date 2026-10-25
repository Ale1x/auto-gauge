import Foundation

public enum GaugeCommitState: Equatable, Sendable {
    case idle, received, appliedVolatile(Int), appliedHostDurable(Int), appliedDeviceDurable(Int), rejected(String), unknown
    public var titleIT: String {
        switch self {
        case .idle: return "Nessuna applicazione in corso"
        case .received: return "Ricevuta dal backend di sviluppo · non ancora applicata"
        case .appliedVolatile(let revision): return "Applicata in RAM simulata · revisione \(revision) · volatile"
        case .appliedHostDurable(let revision): return "Applicata nel file C di sviluppo · revisione \(revision) · durabilità hardware non qualificata"
        case .appliedDeviceDurable(let revision): return "Applicazione confermata dal gauge · revisione \(revision)"
        case .rejected(let reason): return "Rifiutata dal backend di sviluppo: \(reason)"
        case .unknown: return "Esito sconosciuto · riconnetti e rileggi il backend di sviluppo"
        }
    }
}

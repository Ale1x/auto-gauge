import Foundation

public enum GaugeLinkError: Error, LocalizedError {
    case disconnected, notReady, busy, timeout, invalidChunk, invalidReply, exhausted, unqualified, rejected(String), unknownCommit
    public var errorDescription: String? {
        switch self {
        case .disconnected: return "Collegamento Bluetooth interrotto."
        case .notReady: return "Attendi servizio, caratteristiche e conferma delle indicazioni."
        case .busy: return "È già in corso una richiesta."
        case .timeout: return "Tempo massimo del collegamento superato."
        case .invalidChunk: return "Frammento Bluetooth non valido o buffer pieno."
        case .invalidReply: return "Risposta non coerente con la richiesta."
        case .exhausted: return "Identificatori esauriti: riconnetti il dispositivo."
        case .unqualified: return "Mutazioni reali bloccate: sicurezza server e ammissione non ancora qualificate."
        case .rejected(let reason): return "Richiesta rifiutata: \(reason)"
        case .unknownCommit: return "Esito applicazione sconosciuto. Riconnetti e rileggi la configurazione; nessun rollback o nuovo invio automatico."
        }
    }
}

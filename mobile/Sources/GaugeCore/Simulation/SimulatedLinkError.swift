import Foundation

public enum SimulatedLinkError: Error, LocalizedError {
    case busy, disconnected, invalidClient, exhausted, operationInProgress, invalidReply, incompatibleVersion, mustRead, revisionConflict, rejected(String), unknownCommit
    public var errorDescription: String? {
        switch self {
        case .busy: return "Un altro client occupa il collegamento simulato."
        case .disconnected: return "Collegamento simulato non disponibile."
        case .invalidClient: return "Client simulato non valido."
        case .exhausted: return "Identificatori di sessione o richiesta esauriti."
        case .operationInProgress: return "Un’altra operazione simulata è in corso."
        case .invalidReply: return "Risposta simulata non valida o non correlata alla richiesta."
        case .incompatibleVersion: return "Versione del protocollo simulato non compatibile."
        case .mustRead: return "Leggi prima lo stato corrente dal gauge simulato."
        case .revisionConflict: return "La revisione del gauge simulato è cambiata. La bozza è conservata: rileggi lo stato prima di applicarla."
        case .rejected(let reason): return "Richiesta simulata rifiutata: \(reason)."
        case .unknownCommit: return "Esito dell’applicazione sconosciuto: il gauge potrebbe aver applicato la modifica. Ricollega e rileggi lo stato; nessun rollback automatico."
        }
    }
}

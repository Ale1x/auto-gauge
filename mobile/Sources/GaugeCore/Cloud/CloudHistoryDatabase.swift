import Foundation

public enum CloudAccount: Equatable, Sendable {
    case available(context: String), noAccount, restricted, temporarilyUnavailable, notConfigured
}
public enum CloudSyncError: Error, LocalizedError {
    case cursorExpired, invalidData, conflict, accountChanged, unavailable, capacity, notLoaded, busy, simulatedData
    public var errorDescription: String? {
        switch self {
        case .cursorExpired: return "Il cursore iCloud non è più utilizzabile. Avvia una nuova scansione: gli storici recuperati saranno conservati."
        case .invalidData: return "Dati cloud locali non validi."
        case .conflict: return "Uno storico cloud con la stessa identità contiene dati diversi. La coda è conservata."
        case .accountChanged: return "Account iCloud cambiato: operazione interrotta. Le code restano separate per account."
        case .unavailable: return "iCloud non disponibile. La coda locale è conservata."
        case .capacity: return "La coda cloud locale è piena. Nessun dato è stato eliminato."
        case .notLoaded: return "Ricarica la coda cloud prima di continuare."
        case .busy: return "Una sincronizzazione è già in corso."
        case .simulatedData: return "Gli storici simulati non vengono caricati su iCloud."
        }
    }
}
public enum HistoryProvenance: Sendable { case realDevice, simulation }
public protocol CloudHistoryDatabase: Sendable {
    func account() async throws -> CloudAccount
    /// Must create only when absent; identical content is success, different content is conflict.
    /// The supplied account context must match before every remote operation.
    func putImmutable(_ item: CloudHistoryItem, context: String) async throws
}

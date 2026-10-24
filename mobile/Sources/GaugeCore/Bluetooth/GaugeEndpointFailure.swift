import Foundation

/// Typed development diagnostic. Raw source values are for diagnostics/tests, not UI copy.
public struct GaugeEndpointFailure: Error, LocalizedError, Equatable, Sendable {
    public let kind: GaugeEndpointFailureKind
    public let storeState: GaugeEndpointStoreState
    public let operation: GaugeRPCOperation
    public var errorDescription: String? {
        switch kind {
        case .configurationNotOperational:
            return storeState == .readOnly ? "Configurazione non verificabile: archivio in sola lettura. Nessuno stato corrente confermato e nessun aggiornamento automatico." : "Configurazione non verificabile. Lo snapshot precedente resta da rileggere; nessun aggiornamento automatico."
        case .historyNotOperational:
            return "Storico non verificabile: sorgente in sola lettura. Nessun nuovo ACK confermato."
        case .outcomeUnknown:
            return "Esito applicazione non verificabile. Riconnetti e rileggi la configurazione prima di ripetere l’applicazione."
        }
    }
}

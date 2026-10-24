import Foundation

public enum HistoryTimelineError: Error, LocalizedError {
    case capacity, invalidData
    public var errorDescription: String? {
        switch self {
        case .capacity: return "La copia supera i limiti di analisi di questa versione. I dati originali restano conservati."
        case .invalidData: return "La copia contiene identità o sequenze duplicate non analizzabili. Nessun dato originale modificato."
        }
    }
}

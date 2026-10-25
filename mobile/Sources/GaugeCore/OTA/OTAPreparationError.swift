import Foundation

public enum OTAPreparationError: Error, Equatable, LocalizedError, Sendable {
    case trustUnavailable, missingFiles, invalidFile, fileTooLarge, invalidArchive, notVerified
    public var errorDescription: String? {
        switch self {
        case .trustUnavailable: "Verifica non disponibile: manca il profilo locale fidato del gauge."
        case .missingFiles: "Seleziona sia il manifesto sia l’immagine firmware."
        case .invalidFile: "Il file non è leggibile oppure non è un file regolare."
        case .fileTooLarge: "Il file è vuoto o supera il limite consentito."
        case .invalidArchive: "La preparazione salvata è incompleta o non valida."
        case .notVerified: "Verifica nuovamente il pacchetto prima di salvarlo."
        }
    }
}

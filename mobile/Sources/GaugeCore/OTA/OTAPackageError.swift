import Foundation

public enum OTAPackageError: Error, Equatable, LocalizedError, Sendable {
    case envelope, schema, manifest, profile, hardware, replay, capacity, untrustedKey, publicKey, signature, image
    public var errorDescription: String? {
        switch self {
        case .envelope: "Pacchetto non valido o fuori limite."
        case .schema: "Schema del pacchetto non supportato."
        case .manifest: "Manifesto del pacchetto non valido."
        case .profile: "Pacchetto fuori dal profilo candidato ESP32-S3 supportato."
        case .hardware: "Pacchetto destinato a un altro hardware."
        case .replay: "Versione già accettata o precedente alla soglia locale."
        case .capacity: "Immagine troppo grande per la capacità dichiarata."
        case .untrustedKey: "Chiave di firma non presente fra quelle fidate locali."
        case .publicKey: "Chiave pubblica RSA-3072 non valida."
        case .signature: "Firma del pacchetto non valida."
        case .image: "Dimensione o impronta dell’immagine non corrispondente."
        }
    }
}

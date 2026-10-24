import Foundation

public struct HistoryPIDDefinition: Equatable, Identifiable, Sendable {
    public let id: Int
    public let nameIT: String
    public let unit: String
    public let byteCount: Int
    let numerator: Double
    let denominator: Double
    let offset: Double
    /// Qualified scalar catalog from firmware/src/obd_decode.c; bytes exclude Service/PID headers.
    /// Formula availability is distinct from gauge/ECU support discovered at runtime.
    public static let qualified: [Self] = [
        .init(id: 0x04, nameIT: "Carico motore", unit: "%", byteCount: 1, numerator: 100, denominator: 255, offset: 0),
        .init(id: 0x05, nameIT: "Temperatura refrigerante", unit: "°C", byteCount: 1, numerator: 1, denominator: 1, offset: -40),
        .init(id: 0x06, nameIT: "Correzione carburante breve banco 1", unit: "%", byteCount: 1, numerator: 100, denominator: 128, offset: -100),
        .init(id: 0x07, nameIT: "Correzione carburante lunga banco 1", unit: "%", byteCount: 1, numerator: 100, denominator: 128, offset: -100),
        .init(id: 0x08, nameIT: "Correzione carburante breve banco 2", unit: "%", byteCount: 1, numerator: 100, denominator: 128, offset: -100),
        .init(id: 0x09, nameIT: "Correzione carburante lunga banco 2", unit: "%", byteCount: 1, numerator: 100, denominator: 128, offset: -100),
        .init(id: 0x0a, nameIT: "Pressione carburante", unit: "kPa", byteCount: 1, numerator: 3, denominator: 1, offset: 0),
        .init(id: 0x0b, nameIT: "Pressione collettore aspirazione", unit: "kPa", byteCount: 1, numerator: 1, denominator: 1, offset: 0),
        .init(id: 0x0c, nameIT: "Regime motore", unit: "giri/min", byteCount: 2, numerator: 1, denominator: 4, offset: 0),
        .init(id: 0x0d, nameIT: "Velocità", unit: "km/h", byteCount: 1, numerator: 1, denominator: 1, offset: 0),
        .init(id: 0x0e, nameIT: "Anticipo accensione", unit: "°", byteCount: 1, numerator: 1, denominator: 2, offset: -64),
        .init(id: 0x0f, nameIT: "Temperatura aria aspirata", unit: "°C", byteCount: 1, numerator: 1, denominator: 1, offset: -40),
        .init(id: 0x10, nameIT: "Portata aria", unit: "g/s", byteCount: 2, numerator: 1, denominator: 100, offset: 0),
        .init(id: 0x11, nameIT: "Posizione farfalla", unit: "%", byteCount: 1, numerator: 100, denominator: 255, offset: 0),
        .init(id: 0x1f, nameIT: "Tempo dall’avviamento", unit: "s", byteCount: 2, numerator: 1, denominator: 1, offset: 0),
        .init(id: 0x21, nameIT: "Distanza con spia motore accesa", unit: "km", byteCount: 2, numerator: 1, denominator: 1, offset: 0),
        .init(id: 0x22, nameIT: "Pressione rail rispetto al collettore", unit: "kPa", byteCount: 2, numerator: 79, denominator: 1000, offset: 0),
        .init(id: 0x23, nameIT: "Pressione rail carburante", unit: "kPa", byteCount: 2, numerator: 10, denominator: 1, offset: 0),
        .init(id: 0x2c, nameIT: "EGR comandata", unit: "%", byteCount: 1, numerator: 100, denominator: 255, offset: 0),
        .init(id: 0x2d, nameIT: "Errore EGR", unit: "%", byteCount: 1, numerator: 100, denominator: 128, offset: -100),
        .init(id: 0x2e, nameIT: "Spurgo evaporativo comandato", unit: "%", byteCount: 1, numerator: 100, denominator: 255, offset: 0),
        .init(id: 0x2f, nameIT: "Livello carburante", unit: "%", byteCount: 1, numerator: 100, denominator: 255, offset: 0),
        .init(id: 0x30, nameIT: "Riscaldamenti dopo cancellazione errori", unit: "conteggio", byteCount: 1, numerator: 1, denominator: 1, offset: 0),
        .init(id: 0x31, nameIT: "Distanza dopo cancellazione errori", unit: "km", byteCount: 2, numerator: 1, denominator: 1, offset: 0),
        .init(id: 0x33, nameIT: "Pressione atmosferica", unit: "kPa", byteCount: 1, numerator: 1, denominator: 1, offset: 0),
        .init(id: 0x3c, nameIT: "Temperatura catalizzatore banco 1 sensore 1", unit: "°C", byteCount: 2, numerator: 1, denominator: 10, offset: -40),
        .init(id: 0x3d, nameIT: "Temperatura catalizzatore banco 2 sensore 1", unit: "°C", byteCount: 2, numerator: 1, denominator: 10, offset: -40),
        .init(id: 0x3e, nameIT: "Temperatura catalizzatore banco 1 sensore 2", unit: "°C", byteCount: 2, numerator: 1, denominator: 10, offset: -40),
        .init(id: 0x3f, nameIT: "Temperatura catalizzatore banco 2 sensore 2", unit: "°C", byteCount: 2, numerator: 1, denominator: 10, offset: -40),
        .init(id: 0x42, nameIT: "Tensione centralina", unit: "V", byteCount: 2, numerator: 1, denominator: 1000, offset: 0),
        .init(id: 0x43, nameIT: "Carico motore assoluto", unit: "%", byteCount: 2, numerator: 100, denominator: 255, offset: 0),
        .init(id: 0x44, nameIT: "Rapporto aria/carburante equivalente comandato", unit: "rapporto", byteCount: 2, numerator: 1, denominator: 32768, offset: 0),
        .init(id: 0x45, nameIT: "Posizione relativa farfalla", unit: "%", byteCount: 1, numerator: 100, denominator: 255, offset: 0),
        .init(id: 0x46, nameIT: "Temperatura ambiente", unit: "°C", byteCount: 1, numerator: 1, denominator: 1, offset: -40),
        .init(id: 0x47, nameIT: "Posizione farfalla B", unit: "%", byteCount: 1, numerator: 100, denominator: 255, offset: 0),
        .init(id: 0x48, nameIT: "Posizione farfalla C", unit: "%", byteCount: 1, numerator: 100, denominator: 255, offset: 0),
        .init(id: 0x49, nameIT: "Posizione pedale D", unit: "%", byteCount: 1, numerator: 100, denominator: 255, offset: 0),
        .init(id: 0x4a, nameIT: "Posizione pedale E", unit: "%", byteCount: 1, numerator: 100, denominator: 255, offset: 0),
        .init(id: 0x4b, nameIT: "Posizione pedale F", unit: "%", byteCount: 1, numerator: 100, denominator: 255, offset: 0),
        .init(id: 0x4c, nameIT: "Attuatore farfalla comandato", unit: "%", byteCount: 1, numerator: 100, denominator: 255, offset: 0),
        .init(id: 0x4d, nameIT: "Tempo con spia motore accesa", unit: "min", byteCount: 2, numerator: 1, denominator: 1, offset: 0),
        .init(id: 0x4e, nameIT: "Tempo dopo cancellazione errori", unit: "min", byteCount: 2, numerator: 1, denominator: 1, offset: 0),
        .init(id: 0x52, nameIT: "Percentuale etanolo carburante", unit: "%", byteCount: 1, numerator: 100, denominator: 255, offset: 0),
        .init(id: 0x59, nameIT: "Pressione rail assoluta", unit: "kPa", byteCount: 2, numerator: 10, denominator: 1, offset: 0),
        .init(id: 0x5a, nameIT: "Posizione relativa pedale", unit: "%", byteCount: 1, numerator: 100, denominator: 255, offset: 0),
        .init(id: 0x5b, nameIT: "Vita residua pacco ibrido", unit: "%", byteCount: 1, numerator: 100, denominator: 255, offset: 0),
        .init(id: 0x5c, nameIT: "Temperatura olio motore", unit: "°C", byteCount: 1, numerator: 1, denominator: 1, offset: -40),
        .init(id: 0x5d, nameIT: "Fasatura iniezione", unit: "°", byteCount: 2, numerator: 1, denominator: 128, offset: -210),
        .init(id: 0x5e, nameIT: "Consumo carburante orario", unit: "L/h", byteCount: 2, numerator: 1, denominator: 20, offset: 0),
        .init(id: 0x61, nameIT: "Coppia motore richiesta", unit: "%", byteCount: 1, numerator: 1, denominator: 1, offset: -125),
        .init(id: 0x62, nameIT: "Coppia motore effettiva", unit: "%", byteCount: 1, numerator: 1, denominator: 1, offset: -125),
        .init(id: 0x63, nameIT: "Coppia motore di riferimento", unit: "N·m", byteCount: 2, numerator: 1, denominator: 1, offset: 0),
        .init(id: 0xa6, nameIT: "Odometro", unit: "km", byteCount: 4, numerator: 1, denominator: 10, offset: 0)
    ]
    public static func find(_ pid: Int) -> Self? { qualified.first { $0.id == pid } }
    public func value(rawHex: String) -> Double? {
        guard rawHex.utf8.count == byteCount * 2 else { return nil }
        var raw: UInt32 = 0
        for character in rawHex.utf8 {
            let nibble: UInt8
            switch character { case 48...57: nibble = character - 48; case 97...102: nibble = character - 87; default: return nil }
            raw = raw << 4 | UInt32(nibble)
        }
        return Double(raw) * numerator / denominator + offset
    }
}

import Foundation

public struct HistoryPIDDefinition: Equatable, Identifiable, Sendable {
    public let id: Int
    public let nameIT: String
    public let unit: String
    public let byteCount: Int
    let numerator: Double
    let denominator: Double
    let offset: Double
    /// Qualified scalar subset from firmware/src/obd_decode.c; bytes exclude Service/PID headers.
    public static let qualified: [Self] = [
        .init(id: 0x04, nameIT: "Carico motore", unit: "%", byteCount: 1, numerator: 100, denominator: 255, offset: 0),
        .init(id: 0x05, nameIT: "Temperatura refrigerante", unit: "°C", byteCount: 1, numerator: 1, denominator: 1, offset: -40),
        .init(id: 0x0c, nameIT: "Regime motore", unit: "giri/min", byteCount: 2, numerator: 1, denominator: 4, offset: 0),
        .init(id: 0x0d, nameIT: "Velocità", unit: "km/h", byteCount: 1, numerator: 1, denominator: 1, offset: 0),
        .init(id: 0x0f, nameIT: "Temperatura aria aspirata", unit: "°C", byteCount: 1, numerator: 1, denominator: 1, offset: -40),
        .init(id: 0x10, nameIT: "Portata aria", unit: "g/s", byteCount: 2, numerator: 1, denominator: 100, offset: 0),
        .init(id: 0x11, nameIT: "Posizione farfalla", unit: "%", byteCount: 1, numerator: 100, denominator: 255, offset: 0)
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

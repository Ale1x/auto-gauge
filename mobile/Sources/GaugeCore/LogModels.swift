import Foundation

public enum LogImportError: Error, LocalizedError {
    case invalidPage, invalidCounter, invalidSession, conflict, unexplainedGap, lossConflict, capacity, notLoaded, invalidArchive
    public var errorDescription: String? {
        switch self {
        case .invalidPage: return "La pagina storico non rispetta il formato previsto."
        case .invalidCounter: return "Sequenza o tempo relativo non validi."
        case .invalidSession: return "Identificatore della sessione non valido."
        case .conflict: return "Una sequenza già importata contiene dati diversi. Nessuna modifica salvata."
        case .unexplainedGap: return "Mancano sequenze senza una perdita esplicitamente dichiarata."
        case .lossConflict: return "La perdita dichiarata contraddice record già importati."
        case .capacity: return "L’archivio dimostrativo ha raggiunto il limite locale. Nessun nuovo ACK emesso."
        case .notLoaded: return "Ricarica correttamente l’archivio locale prima di importare."
        case .invalidArchive: return "L’archivio locale non è valido. Il contenuto precedente è stato conservato."
        }
    }
}

public struct LogUInt64: Codable, Equatable, Comparable, Sendable {
    public let value: UInt64
    public init(_ value: UInt64) { self.value = value }
    public var decimal: String { String(value) }
    public static func < (lhs: Self, rhs: Self) -> Bool { lhs.value < rhs.value }
    public init(from decoder: Decoder) throws {
        let text = try decoder.singleValueContainer().decode(String.self)
        guard (1...20).contains(text.utf8.count), text.utf8.allSatisfy({ (48...57).contains($0) }),
              let value = UInt64(text), String(value) == text else { throw LogImportError.invalidCounter }
        self.value = value
    }
    public func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        try container.encode(decimal)
    }
}

public struct LogSessionID: Codable, Hashable, Sendable {
    public let value: String
    public init(_ value: String) throws {
        guard value.utf8.count == 32, value.utf8.allSatisfy({ (48...57).contains($0) || (97...102).contains($0) }),
              value.contains(where: { $0 != "0" }) else { throw LogImportError.invalidSession }
        self.value = value
    }
    public init(from decoder: Decoder) throws { try self.init(decoder.singleValueContainer().decode(String.self)) }
    public func encode(to encoder: Encoder) throws { var c = encoder.singleValueContainer(); try c.encode(value) }
}

public enum LogRecordStatus: String, Codable, Sendable {
    case valid, unsupported, unavailable, stale, timeout, error
    public var titleIT: String {
        switch self {
        case .valid: return "Valido"
        case .unsupported: return "Non supportato"
        case .unavailable: return "Non disponibile"
        case .stale: return "Scaduto"
        case .timeout: return "Tempo scaduto"
        case .error: return "Errore"
        }
    }
}

private struct LogCodingKey: CodingKey {
    let stringValue: String
    var intValue: Int? { nil }
    init?(stringValue: String) { self.stringValue = stringValue }
    init?(intValue: Int) { return nil }
}

private func requireLogKeys(_ decoder: Decoder, required: Set<String>, optional: Set<String> = []) throws {
    let keys = Set(try decoder.container(keyedBy: LogCodingKey.self).allKeys.map(\.stringValue))
    guard required.isSubset(of: keys), keys.isSubset(of: required.union(optional)) else { throw LogImportError.invalidPage }
}

public struct HistoricalLogRecord: Codable, Equatable, Identifiable, Sendable {
    public let sequence: LogUInt64
    public let monotonicMS: LogUInt64
    public let pid: Int
    public let ecuSource: UInt32
    public let status: LogRecordStatus
    public let rawHex: String
    public var id: String { sequence.decimal }
    enum CodingKeys: String, CodingKey { case sequence, pid, status; case monotonicMS = "monotonic_ms", ecuSource = "ecu_source", rawHex = "raw_hex" }
    public init(sequence: UInt64, monotonicMS: UInt64, pid: Int, ecuSource: UInt32, status: LogRecordStatus, rawHex: String) throws {
        guard sequence > 0, (1...255).contains(pid), ecuSource <= 0x1fffffff,
              rawHex.utf8.count <= 128, rawHex.utf8.count % 2 == 0,
              rawHex.utf8.allSatisfy({ (48...57).contains($0) || (97...102).contains($0) }),
              (status == .valid) == !rawHex.isEmpty else { throw LogImportError.invalidPage }
        self.sequence = .init(sequence); self.monotonicMS = .init(monotonicMS)
        self.pid = pid; self.ecuSource = ecuSource; self.status = status; self.rawHex = rawHex
    }
    public init(from decoder: Decoder) throws {
        try requireLogKeys(decoder, required: ["sequence", "monotonic_ms", "pid", "ecu_source", "status", "raw_hex"])
        let c = try decoder.container(keyedBy: CodingKeys.self)
        try self.init(sequence: c.decode(LogUInt64.self, forKey: .sequence).value,
                      monotonicMS: c.decode(LogUInt64.self, forKey: .monotonicMS).value,
                      pid: c.decode(Int.self, forKey: .pid), ecuSource: c.decode(UInt32.self, forKey: .ecuSource),
                      status: c.decode(LogRecordStatus.self, forKey: .status), rawHex: c.decode(String.self, forKey: .rawHex))
    }
    /// Never converts a monotonic counter into a wall-clock date or Double.
    public var relativeTimeIT: String { "+\(monotonicMS.value / 1000) s · \(monotonicMS.value % 1000) ms dall’avvio" }
}

public struct LogLossInterval: Codable, Equatable, Identifiable, Sendable {
    public let first: LogUInt64
    public let last: LogUInt64
    public var id: String { "\(first.decimal)-\(last.decimal)" }
    public init(first: UInt64, last: UInt64) throws {
        guard first > 0, last >= first else { throw LogImportError.invalidPage }
        self.first = .init(first); self.last = .init(last)
    }
    enum CodingKeys: String, CodingKey { case first, last }
    public init(from decoder: Decoder) throws {
        try requireLogKeys(decoder, required: ["first", "last"])
        let c = try decoder.container(keyedBy: CodingKeys.self)
        try self.init(first: c.decode(LogUInt64.self, forKey: .first).value, last: c.decode(LogUInt64.self, forKey: .last).value)
    }
    func contains(_ sequence: UInt64) -> Bool { first.value <= sequence && sequence <= last.value }
}

public struct HistoricalLogPage: Codable, Equatable, Sendable {
    public let sessionID: LogSessionID
    public let records: [HistoricalLogRecord]
    public let lost: [LogLossInterval]
    enum CodingKeys: String, CodingKey { case sessionID = "session_id", records, lost }
    public init(sessionID: LogSessionID, records: [HistoricalLogRecord], lost: [LogLossInterval] = []) throws {
        guard records.count <= 16, lost.count <= 16 else { throw LogImportError.capacity }
        let intervals = lost.sorted { $0.first < $1.first }
        guard zip(intervals, intervals.dropFirst()).allSatisfy({ pair in pair.0.last < pair.1.first }),
              zip(records, records.dropFirst()).allSatisfy({ pair in pair.0.sequence < pair.1.sequence }),
              records.allSatisfy({ record in !lost.contains(where: { $0.contains(record.sequence.value) }) }) else { throw LogImportError.invalidPage }
        self.sessionID = sessionID; self.records = records; self.lost = lost
    }
    public init(from decoder: Decoder) throws {
        try requireLogKeys(decoder, required: ["session_id", "records"], optional: ["lost"])
        let c = try decoder.container(keyedBy: CodingKeys.self)
        try self.init(sessionID: c.decode(LogSessionID.self, forKey: .sessionID), records: c.decode([HistoricalLogRecord].self, forKey: .records), lost: c.contains(.lost) ? c.decode([LogLossInterval].self, forKey: .lost) : [])
    }
    public static func decode(_ data: Data) throws -> Self {
        guard data.count <= 4096 else { throw LogImportError.capacity }
        try StrictLogJSON.check(data)
        return try JSONDecoder().decode(Self.self, from: data)
    }
}

/// Checks duplicate keys and depth before Codable decoding. Input is already size-bounded.
/// JSONDecoder performs the complete grammar/type validation afterwards.
enum StrictLogJSON {
    static func check(_ data: Data) throws {
        enum Scope { case object(Set<String>), array }
        let bytes = Array(data)
        var stack = [Scope](), i = 0
        func whitespace(_ byte: UInt8) -> Bool { [9, 10, 13, 32].contains(byte) }
        while i < bytes.count {
            switch bytes[i] {
            case 123: stack.append(.object([]))
            case 91: stack.append(.array)
            case 125, 93: if !stack.isEmpty { stack.removeLast() }
            case 45, 48...57:
                let start = i
                while i + 1 < bytes.count, !whitespace(bytes[i + 1]), ![44, 93, 125].contains(bytes[i + 1]) { i += 1 }
                let token = bytes[start...i]
                let digits = token.first == 45 ? token.dropFirst() : token
                guard !digits.isEmpty, digits.allSatisfy({ (48...57).contains($0) }), digits.count == 1 || digits.first != 48 else { throw LogImportError.invalidPage }
            case 34:
                let start = i
                i += 1
                var closed = false
                while i < bytes.count {
                    if bytes[i] == 92 { i += 2; continue }
                    if bytes[i] == 34 { closed = true; break }
                    i += 1
                }
                guard closed else { throw LogImportError.invalidPage }
                var next = i + 1
                while next < bytes.count, whitespace(bytes[next]) { next += 1 }
                if next < bytes.count, bytes[next] == 58 {
                    guard let scope = stack.popLast(), case .object(var keys) = scope else { throw LogImportError.invalidPage }
                    let key = try JSONDecoder().decode(String.self, from: Data(bytes[start...i]))
                    guard keys.insert(key).inserted else { throw LogImportError.invalidPage }
                    stack.append(.object(keys))
                }
            default: break
            }
            guard stack.count <= 8 else { throw LogImportError.invalidPage }
            i += 1
        }
    }
}

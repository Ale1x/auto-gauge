import Foundation

struct OTAStrictJSONParser {
    let bytes: [UInt8]
    var index = 0
    mutating func whitespace() { while index < bytes.count && [9,10,13,32].contains(bytes[index]) { index += 1 } }
    mutating func value(depth:Int) throws -> OTAStrictJSON {
        whitespace()
        guard depth < 4, index < bytes.count else { throw OTAPackageError.envelope }
        if bytes[index] == 34 { return .string(try string()) }
        if bytes[index] == 123 {
            index += 1; whitespace(); var fields: [String:OTAStrictJSON] = [:]
            if consume(125) { return .object(fields) }
            while true {
                whitespace(); let key = try string(); whitespace()
                guard fields.count < 16, fields[key] == nil, consume(58) else { throw OTAPackageError.envelope }
                fields[key] = try value(depth:depth+1); whitespace()
                if consume(125) { return .object(fields) }
                guard consume(44) else { throw OTAPackageError.envelope }
            }
        }
        let start = index
        _ = consume(45)
        guard index < bytes.count else { throw OTAPackageError.envelope }
        if !consume(48) {
            guard (49...57).contains(bytes[index]) else { throw OTAPackageError.envelope }
            while index < bytes.count && (48...57).contains(bytes[index]) { index += 1 }
        }
        return .integer(String(decoding:bytes[start..<index],as:UTF8.self))
    }
    mutating func consume(_ byte:UInt8) -> Bool {
        guard index < bytes.count, bytes[index] == byte else { return false }
        index += 1; return true
    }
    mutating func string() throws -> String {
        let start = index
        guard consume(34) else { throw OTAPackageError.envelope }
        var escaped = false
        while index < bytes.count {
            let byte = bytes[index]; index += 1
            if escaped { escaped = false; continue }
            if byte == 92 { escaped = true; continue }
            if byte == 34 {
                do { return try JSONDecoder().decode(String.self,from:Data(bytes[start..<index])) }
                catch { throw OTAPackageError.envelope }
            }
        }
        throw OTAPackageError.envelope
    }
}

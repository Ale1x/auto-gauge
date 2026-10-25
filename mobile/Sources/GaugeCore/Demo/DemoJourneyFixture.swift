import Foundation

/// Self-contained sample journeys. These never carry qualified-device receipts.
public enum DemoJourneyFixture {
    public static let deviceID = "AUTOGAUGE-DEMO"
    public static var trips: [DemoTripMetadata] {
        get throws {
            [try .init(sessionID: LogSessionID(String(repeating: "a1", count: 16)), title: "Verso il lavoro", date: Date(timeIntervalSince1970: 1_792_819_800), durationSeconds: 480),
             try .init(sessionID: LogSessionID(String(repeating: "b2", count: 16)), title: "Rientro a casa", date: Date(timeIntervalSince1970: 1_792_851_300), durationSeconds: 720)]
        }
    }
    public static func pages(for trip: DemoTripMetadata) throws -> [Data] {
        var records: [HistoricalLogRecord] = []
        var sequence: UInt64 = 1
        for second in stride(from: 0, to: trip.durationSeconds, by: 5) {
            let wave = sin(Double(second) / 35)
            let speed = max(0, min(110, Int(48 + 33 * wave)))
            let rpm = max(760, 1100 + speed * 24 + Int(110 * sin(Double(second) / 9)))
            let temperature = min(92, 48 + second / 9)
            let load = max(4, min(92, 30 + Int(22 * wave)))
            let values: [(Int, [UInt8])] = [
                (12, [UInt8((rpm * 4) >> 8), UInt8((rpm * 4) & 255)]),
                (13, [UInt8(speed)]), (5, [UInt8(temperature + 40)]),
                (4, [UInt8(load * 255 / 100)])
            ]
            for (pid, raw) in values {
                let interrupted = second == 205 && pid == 13
                let hex = raw.map { let value = String($0, radix: 16); return value.count == 1 ? "0" + value : value }.joined()
                records.append(try .init(sequence: sequence, monotonicMS: UInt64(second * 1000), pid: pid,
                    ecuSource: 0x7e8, status: interrupted ? .unavailable : .valid, rawHex: interrupted ? "" : hex))
                sequence += 1
            }
        }
        return try stride(from: 0, to: records.count, by: 16).map { start in
            try JSONEncoder().encode(HistoricalLogPage(sessionID: trip.sessionID, records: Array(records.dropFirst(start).prefix(16))))
        }
    }
}

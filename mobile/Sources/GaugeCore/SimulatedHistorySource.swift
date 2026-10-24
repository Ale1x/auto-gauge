import Foundation

/// Immutable snapshot of a bounded volatile ring. ACK never mutates or deletes it.
public struct SimulatedHistorySource: Sendable {
    public let sessionID: LogSessionID
    public let records: [HistoricalLogRecord]
    public let lastSequence: UInt64
    private let emptyFirstSequence: UInt64
    public init(sessionID: LogSessionID, records: [HistoricalLogRecord], capacity: Int = 64, firstSequence: UInt64 = 1) throws {
        guard (1...64).contains(capacity), records.count <= 64, firstSequence > 0 else { throw LogImportError.capacity }
        var expected = firstSequence, lastMS: UInt64 = 0
        for (index, record) in records.enumerated() {
            guard record.sequence.value == expected, record.monotonicMS.value >= lastMS else { throw LogImportError.invalidPage }
            lastMS = record.monotonicMS.value
            if index < records.count - 1 {
                guard expected < UInt64.max else { throw LogImportError.invalidCounter }
                expected += 1
            }
        }
        self.sessionID = sessionID; self.records = Array(records.suffix(capacity))
        lastSequence = records.last?.sequence.value ?? (firstSequence - 1)
        emptyFirstSequence = firstSequence
    }
    func losses(after cursor: UInt64) throws -> [LogLossInterval] {
        let first = records.first?.sequence.value ?? emptyFirstSequence
        if cursor < first - 1 { return [try .init(first: cursor + 1, last: first - 1)] }
        return []
    }
}

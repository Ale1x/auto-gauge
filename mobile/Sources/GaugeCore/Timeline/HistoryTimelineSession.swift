import Foundation

public struct HistoryTimelineSession: Equatable, Identifiable, Sendable {
    public let deviceID: String
    public let sessionID: LogSessionID
    public let source: HistoryTimelineSource
    public let records: [HistoricalLogRecord]
    public let losses: [LogLossInterval]
    public var id: String { "\(deviceID):\(sessionID.value)" }
    public init(deviceID: String, sessionID: LogSessionID, source: HistoryTimelineSource, records: [HistoricalLogRecord], losses: [LogLossInterval]) throws {
        guard records.count <= 1024, losses.count <= 1024 else { throw HistoryTimelineError.capacity }
        guard !deviceID.isEmpty, deviceID.utf8.count <= 256, Set(losses.map(\.id)).count == losses.count,
              Set(records.map { $0.sequence.value }).count == records.count else { throw HistoryTimelineError.invalidData }
        self.deviceID = deviceID; self.sessionID = sessionID; self.source = source
        self.records = records.sorted { $0.sequence < $1.sequence }; self.losses = losses.sorted { $0.first < $1.first }
    }
    public static func imported(_ sessions: [ImportedLogSession]) throws -> [Self] {
        guard sessions.count <= 16, sessions.reduce(0, { $0 + $1.records.count }) <= 1024 else { throw HistoryTimelineError.capacity }
        return try sessions.map { try .init(deviceID: $0.deviceID, sessionID: $0.sessionID, source: .simulatedGauge, records: $0.records, losses: $0.lost) }
    }
    public static func cloud(_ items: [CloudHistoryItem]) throws -> [Self] {
        guard items.count <= 1024 else { throw HistoryTimelineError.capacity }
        let groups = Dictionary(grouping: items) { "\($0.deviceID):\($0.sessionID.value)" }
        return try groups.values.compactMap { group in
            guard let first = group.first else { return nil }
            return try .init(deviceID: first.deviceID, sessionID: first.sessionID, source: .restoredCloud, records: group.compactMap(\.record), losses: group.compactMap(\.loss))
        }.sorted { $0.id < $1.id }
    }
}

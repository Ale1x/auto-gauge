import Foundation
import CryptoKit

/// Immutable schema 1 envelope. Counters inside record/loss retain their string Codable form.
public struct CloudHistoryItem: Codable, Equatable, Sendable {
    public let schemaVersion: Int
    public let deviceID: String
    public let sessionID: LogSessionID
    public let record: HistoricalLogRecord?
    public let loss: LogLossInterval?
    public init(deviceID: String, sessionID: LogSessionID, record: HistoricalLogRecord) throws {
        try self.init(deviceID: deviceID, sessionID: sessionID, record: record, loss: nil)
    }
    public init(deviceID: String, sessionID: LogSessionID, loss: LogLossInterval) throws {
        try self.init(deviceID: deviceID, sessionID: sessionID, record: nil, loss: loss)
    }
    private init(deviceID: String, sessionID: LogSessionID, record: HistoricalLogRecord?, loss: LogLossInterval?) throws {
        guard !deviceID.isEmpty, deviceID.utf8.count <= 128, !deviceID.contains(where: { $0.isNewline || $0.isWhitespace }) else { throw CloudSyncError.invalidData }
        schemaVersion = 1; self.deviceID = deviceID; self.sessionID = sessionID; self.record = record; self.loss = loss
    }
    enum CodingKeys: String, CodingKey, CaseIterable { case schemaVersion, deviceID, sessionID, record, loss }
    public init(from decoder: Decoder) throws {
        let keys = try decoder.container(keyedBy: CloudItemKey.self).allKeys.map(\.stringValue)
        guard Set(keys).isSubset(of: Set(CodingKeys.allCases.map(\.rawValue))) else { throw CloudSyncError.invalidData }
        let c = try decoder.container(keyedBy: CodingKeys.self)
        guard try c.decode(Int.self, forKey: .schemaVersion) == 1 else { throw CloudSyncError.invalidData }
        let record = try c.decodeIfPresent(HistoricalLogRecord.self, forKey: .record)
        let loss = try c.decodeIfPresent(LogLossInterval.self, forKey: .loss)
        guard (record == nil) != (loss == nil) else { throw CloudSyncError.invalidData }
        try self.init(deviceID: c.decode(String.self, forKey: .deviceID), sessionID: c.decode(LogSessionID.self, forKey: .sessionID), record: record, loss: loss)
    }
    public var recordID: String {
        let suffix = record.map { "record:\($0.sequence.decimal)" } ?? "loss:\(loss?.first.decimal ?? "invalid"):\(loss?.last.decimal ?? "invalid")"
        return "ag1-" + Self.digest(Data("\(deviceID.utf8.count):\(deviceID):\(sessionID.value):\(suffix)".utf8))
    }
    public func payload() throws -> Data {
        let encoder = JSONEncoder(); encoder.outputFormatting = [.sortedKeys, .withoutEscapingSlashes]
        return try encoder.encode(self)
    }
    public static func digest(_ data: Data) -> String { SHA256.hash(data: data).map { String(format: "%02x", $0) }.joined() }
    public static func decode(_ data: Data) throws -> Self {
        guard data.count <= 4096 else { throw CloudSyncError.invalidData }
        try StrictLogJSON.check(data)
        let value = try JSONDecoder().decode(Self.self, from: data)
        guard value.schemaVersion == 1, (value.record == nil) != (value.loss == nil) else { throw CloudSyncError.invalidData }
        _ = try Self(deviceID: value.deviceID, sessionID: value.sessionID, record: value.record, loss: value.loss)
        return value
    }
}

private struct CloudItemKey: CodingKey {
    let stringValue: String
    let intValue: Int? = nil
    init?(stringValue: String) { self.stringValue = stringValue }
    init?(intValue: Int) { return nil }
}

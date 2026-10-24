import Foundation

public protocol LogArchiveStorage: Sendable {
    func read(maximumBytes: Int) throws -> Data?
    func write(_ data: Data) throws
}

public struct AtomicLogFileStorage: LogArchiveStorage {
    public let fileURL: URL
    public init(fileURL: URL) { self.fileURL = fileURL }
    public func read(maximumBytes: Int) throws -> Data? {
        guard FileManager.default.fileExists(atPath: fileURL.path) else { return nil }
        let file = try FileHandle(forReadingFrom: fileURL)
        defer { try? file.close() }
        let data = try file.read(upToCount: maximumBytes + 1) ?? Data()
        guard data.count <= maximumBytes else { throw LogImportError.capacity }
        return data
    }
    public func write(_ data: Data) throws {
        guard data.count <= LocalLogArchive.maximumBytes else { throw LogImportError.capacity }
        try FileManager.default.createDirectory(at: fileURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        try data.write(to: fileURL, options: .atomic)
    }
}

public struct ImportedLogSession: Codable, Equatable, Identifiable, Sendable {
    public let deviceID: String
    public let sessionID: LogSessionID
    public fileprivate(set) var acknowledgedThrough: LogUInt64
    public fileprivate(set) var records: [HistoricalLogRecord]
    public fileprivate(set) var lost: [LogLossInterval]
    public var id: String { "\(deviceID):\(sessionID.value)" }
    enum CodingKeys: String, CodingKey {
        case deviceID = "device_id", sessionID = "session_id", acknowledgedThrough = "acknowledged_through", records, lost
    }
}

public struct HistoricalLogAcknowledgement: Codable, Equatable, Sendable {
    public let sessionID: LogSessionID
    public let acknowledgedThrough: LogUInt64
    public let reportedLoss: [LogLossInterval]
    enum CodingKeys: String, CodingKey { case sessionID = "session_id", acknowledgedThrough = "acknowledged_through", reportedLoss = "reported_loss" }
}

private struct HistoricalLogSnapshot: Codable, Equatable, Sendable {
    var schemaVersion = 1
    var sessions = [ImportedLogSession]()
}

/// One actor owns one archive file on one installation. No cross-process/file locking.
/// Other phones have separate archives/cursors; an ACK never erases device records.
public actor LocalLogArchive {
    public static let maximumBytes = 512 * 1024
    public static let maximumRecords = 1024
    public static let maximumSessions = 16
    public static let maximumLossIntervals = 128
    private let storage: any LogArchiveStorage
    private var snapshot = HistoricalLogSnapshot()
    private var ready = false
    public init(storage: any LogArchiveStorage) { self.storage = storage }
    public func sessions() -> [ImportedLogSession] { snapshot.sessions }
    public func isReady() -> Bool { ready }
    public func cursor(deviceID: String, sessionID: String) -> UInt64 {
        snapshot.sessions.first(where: { $0.deviceID == deviceID && $0.sessionID.value == sessionID })?.acknowledgedThrough.value ?? 0
    }
    public func reload() throws {
        do {
            if let data = try storage.read(maximumBytes: Self.maximumBytes) {
                guard data.count <= Self.maximumBytes else { throw LogImportError.capacity }
                try StrictLogJSON.check(data)
                let candidate = try JSONDecoder().decode(HistoricalLogSnapshot.self, from: data)
                try validateSnapshot(candidate)
                snapshot = candidate
            }
            ready = true
        } catch {
            ready = false
            throw error
        }
    }
    public func importPage(deviceID: String, data: Data) throws -> HistoricalLogAcknowledgement {
        guard ready else { throw LogImportError.notLoaded }
        guard validDeviceID(deviceID) else { throw LogImportError.invalidPage }
        let page = try HistoricalLogPage.decode(data)
        var candidate = snapshot
        let index: Int
        if let existing = candidate.sessions.firstIndex(where: { $0.deviceID == deviceID && $0.sessionID == page.sessionID }) {
            index = existing
        } else {
            guard candidate.sessions.count < Self.maximumSessions else { throw LogImportError.capacity }
            candidate.sessions.append(.init(deviceID: deviceID, sessionID: page.sessionID, acknowledgedThrough: .init(0), records: [], lost: []))
            index = candidate.sessions.count - 1
        }
        var session = candidate.sessions[index]
        let current = session.acknowledgedThrough.value
        for interval in page.lost {
            guard !session.records.contains(where: { interval.contains($0.sequence.value) }) else { throw LogImportError.lossConflict }
        }
        var bySequence = Dictionary(uniqueKeysWithValues: session.records.map { ($0.sequence.value, $0) })
        for record in page.records {
            if let old = bySequence[record.sequence.value] {
                guard old == record else { throw LogImportError.conflict }
            } else {
                guard record.sequence.value > current else { throw LogImportError.lossConflict }
                bySequence[record.sequence.value] = record
            }
        }
        let incoming = page.lost + (try page.records.filter { $0.sequence.value > current }.map {
            try LogLossInterval(first: $0.sequence.value, last: $0.sequence.value)
        })
        let cursor = try coveredThrough(incoming, startingAt: current)
        session.records = bySequence.values.sorted { $0.sequence < $1.sequence }
        session.lost = try mergedLosses(session.lost + page.lost)
        session.acknowledgedThrough = .init(cursor)
        candidate.sessions[index] = session
        try validateSnapshot(candidate)
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.sortedKeys]
        let encoded = try encoder.encode(candidate)
        guard encoded.count <= Self.maximumBytes else { throw LogImportError.capacity }
        // Nothing mutates the active snapshot, and no ACK is returned, before write succeeds.
        try storage.write(encoded)
        snapshot = candidate
        return .init(sessionID: page.sessionID, acknowledgedThrough: .init(cursor), reportedLoss: page.lost)
    }
    private func validDeviceID(_ value: String) -> Bool {
        (1...64).contains(value.unicodeScalars.count) && value.utf8.count <= 256
    }
    private func coveredThrough(_ intervals: [LogLossInterval], startingAt start: UInt64) throws -> UInt64 {
        var cursor = start
        for interval in intervals.sorted(by: { $0.first < $1.first }) {
            if interval.first.value > cursor {
                guard cursor < UInt64.max, interval.first.value == cursor + 1 else { throw LogImportError.unexplainedGap }
            }
            cursor = max(cursor, interval.last.value)
        }
        return cursor
    }
    private func mergedLosses(_ intervals: [LogLossInterval]) throws -> [LogLossInterval] {
        var result = [LogLossInterval]()
        for interval in intervals.sorted(by: { $0.first < $1.first }) {
            if let last = result.last,
               interval.first.value <= last.last.value || (last.last.value < UInt64.max && interval.first.value == last.last.value + 1) {
                result[result.count - 1] = try .init(first: last.first.value, last: max(last.last.value, interval.last.value))
            } else { result.append(interval) }
        }
        return result
    }
    private func validateSnapshot(_ value: HistoricalLogSnapshot) throws {
        guard value.schemaVersion == 1, value.sessions.count <= Self.maximumSessions,
              value.sessions.reduce(0, { $0 + $1.records.count }) <= Self.maximumRecords,
              value.sessions.reduce(0, { $0 + $1.lost.count }) <= Self.maximumLossIntervals,
              Set(value.sessions.map(\.id)).count == value.sessions.count else { throw LogImportError.capacity }
        for session in value.sessions {
            guard validDeviceID(session.deviceID), session.records.count <= Self.maximumRecords,
                  zip(session.records, session.records.dropFirst()).allSatisfy({ pair in pair.0.sequence < pair.1.sequence }),
                  zip(session.lost, session.lost.dropFirst()).allSatisfy({ pair in pair.0.last < pair.1.first }),
                  session.records.allSatisfy({ record in !session.lost.contains(where: { $0.contains(record.sequence.value) }) }) else { throw LogImportError.invalidArchive }
            let ranges = session.lost + (try session.records.map { try LogLossInterval(first: $0.sequence.value, last: $0.sequence.value) })
            guard try coveredThrough(ranges, startingAt: 0) == session.acknowledgedThrough.value else { throw LogImportError.invalidArchive }
        }
    }
}

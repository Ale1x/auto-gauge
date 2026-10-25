import Foundation

/// Demo-only local files. No network, account, qualified receipt or OTA provider
/// exists in this type. A separate directory is supplied by the app composition.
public actor DemoExperienceStore {
    private let directory: URL
    public init(directory: URL) { self.directory = directory }
    public func backupSnapshot() throws -> DemoBackupSnapshot {
        guard let data = try read("backup-v1.json") else { return .init() }
        let value = try JSONDecoder().decode(DemoBackupSnapshot.self, from: data)
        try validate(value.sessions); return value
    }
    public func synchronize(_ sessions: [ImportedLogSession], at date: Date = .now) throws -> DemoBackupSnapshot {
        try Task.checkCancellation(); try validate(sessions)
        let snapshot = DemoBackupSnapshot(lastBackupDate: date, sessions: sessions)
        try write(JSONEncoder().encode(snapshot), name: "backup-v1.json")
        return snapshot
    }
    public func restore(into archive: LocalLogArchive) async throws -> DemoBackupSnapshot {
        let snapshot = try backupSnapshot()
        for session in snapshot.sessions {
            // Restore in small protocol-valid pages with the explicit loss map.
            var offset = 0
            repeat {
                try Task.checkCancellation()
                let records = Array(session.records.dropFirst(offset).prefix(16))
                let page = try HistoricalLogPage(sessionID: session.sessionID, records: records, lost: session.lost)
                _ = try await archive.importPage(deviceID: DemoJourneyFixture.deviceID, data: JSONEncoder().encode(page))
                offset += records.count
            } while offset < session.records.count
        }
        return snapshot
    }
    public func firmwareVersion() throws -> String {
        guard let data = try read("firmware-v1.json") else { return "0.13.0" }
        let value = try JSONDecoder().decode(String.self, from: data)
        guard value == "0.13.0" || value == "0.14.0" else { throw LogImportError.invalidArchive }
        return value
    }
    public func installFirmwareVersion(_ value: String) throws {
        try Task.checkCancellation()
        guard value == "0.14.0" else { throw LogImportError.invalidArchive }
        try write(JSONEncoder().encode(value), name: "firmware-v1.json")
    }
    private func validate(_ sessions: [ImportedLogSession]) throws {
        guard sessions.count <= LocalLogArchive.maximumSessions,
              sessions.reduce(0, { $0 + $1.records.count }) <= LocalLogArchive.maximumRecords,
              Set(sessions.map(\.id)).count == sessions.count,
              sessions.allSatisfy({ $0.deviceID == DemoJourneyFixture.deviceID && $0.lost.isEmpty }) else { throw LogImportError.invalidArchive }
        _ = try HistoryTimelineSession.imported(sessions)
    }
    private func read(_ name: String) throws -> Data? {
        try AtomicLogFileStorage(fileURL: directory.appending(path: name)).read(maximumBytes: LocalLogArchive.maximumBytes)
    }
    private func write(_ data: Data, name: String) throws {
        try AtomicLogFileStorage(fileURL: directory.appending(path: name)).write(data)
    }
}

import Foundation

public struct DemoBackupSnapshot: Codable, Sendable {
    public var lastBackupDate: Date?
    public var sessions: [ImportedLogSession]
    public init(lastBackupDate: Date? = nil, sessions: [ImportedLogSession] = []) {
        self.lastBackupDate = lastBackupDate; self.sessions = sessions
    }
}

import Foundation

/// Independent from gauge import: observed source losses never reject cloud records covering them.
public actor CloudHistoryRestoreStore {
    private struct AccountSnapshot: Codable {
        var items: [CloudHistoryItem] = []
        var cursor: Data?
        var scanFinished = false
    }
    private struct Snapshot: Codable {
        var schemaVersion = 1
        var accounts: [String: AccountSnapshot] = [:]
    }
    public static let maximumBytes = 512 * 1024
    public static let maximumItems = 1024
    public static let maximumPagesPerAction = 8
    private let storage: any LogArchiveStorage
    private var snapshot = Snapshot()
    private var loaded = false
    private var context: String?
    private var generation: UInt64 = 0
    private var busy = false
    public init(storage: any LogArchiveStorage) { self.storage = storage }
    public func reload() throws {
        generation &+= 1
        do {
            var candidate = Snapshot()
            if let data = try storage.read(maximumBytes: Self.maximumBytes) {
                guard data.count <= Self.maximumBytes else { throw CloudSyncError.capacity }
                try StrictLogJSON.check(data)
                candidate = try JSONDecoder().decode(Snapshot.self, from: data)
                try validate(candidate)
            }
            snapshot = candidate; loaded = true
        } catch { loaded = false; throw error }
    }
    public func selectAccount(_ value: String?) throws {
        if let value { guard !value.isEmpty, value.utf8.count <= 128 else { throw CloudSyncError.invalidData } }
        if context != value { context = value; generation &+= 1 }
    }
    public func invalidateAccount() { context = nil; generation &+= 1 }
    public func progress() -> CloudRestoreProgress {
        let account = context.flatMap { snapshot.accounts[$0] } ?? AccountSnapshot()
        return .init(items: account.items, scanFinished: account.scanFinished, hasCheckpoint: account.cursor != nil)
    }
    /// Explicit restart retains every restored item, including observations of loss.
    public func restartScan() throws {
        guard loaded else { throw CloudSyncError.notLoaded }
        guard !busy else { throw CloudSyncError.busy }
        guard let context else { throw CloudSyncError.unavailable }
        var candidate = snapshot
        var account = candidate.accounts[context] ?? AccountSnapshot()
        account.cursor = nil; account.scanFinished = false; candidate.accounts[context] = account
        try persist(candidate); generation &+= 1
    }
    public func restore(database: any CloudHistoryPullDatabase, maximumPages: Int = 8) async throws -> CloudRestoreProgress {
        guard loaded else { throw CloudSyncError.notLoaded }
        guard !busy else { throw CloudSyncError.busy }
        guard (1...Self.maximumPagesPerAction).contains(maximumPages) else { throw CloudSyncError.capacity }
        guard let context else { throw CloudSyncError.unavailable }
        busy = true; defer { busy = false }
        let token = generation
        try await verifyAccount(database, context: context, token: token)
        for _ in 0..<maximumPages {
            let account = snapshot.accounts[context] ?? AccountSnapshot()
            if account.scanFinished { break }
            let page = try await database.readHistory(context: context, cursor: account.cursor, limit: CloudHistoryPullPage.maximumRecords)
            try Task.checkCancellation()
            guard generation == token, self.context == context else { throw CloudSyncError.accountChanged }
            try await verifyAccount(database, context: context, token: token)
            guard page.nextCursor == nil || page.nextCursor != account.cursor else { throw CloudSyncError.invalidData }
            var merged = account
            for remote in page.records {
                let item = try remote.validatedItem()
                if let old = merged.items.first(where: { $0.recordID == item.recordID }) {
                    guard old == item else { throw CloudSyncError.conflict }
                } else { merged.items.append(item) }
            }
            merged.cursor = page.nextCursor; merged.scanFinished = page.nextCursor == nil
            var candidate = snapshot; candidate.accounts[context] = merged
            // Cursor and records become visible together, only after durable storage succeeds.
            try persist(candidate)
        }
        return progress()
    }
    private func verifyAccount(_ database: any CloudHistoryDatabase, context: String, token: UInt64) async throws {
        let result = try await database.account()
        try Task.checkCancellation()
        guard generation == token, self.context == context else { throw CloudSyncError.accountChanged }
        guard result == .available(context: context) else { invalidateAccount(); throw CloudSyncError.accountChanged }
    }
    private func persist(_ candidate: Snapshot) throws {
        try validate(candidate)
        let data = try JSONEncoder().encode(candidate)
        guard data.count <= Self.maximumBytes else { throw CloudSyncError.capacity }
        try storage.write(data); snapshot = candidate
    }
    private func validate(_ candidate: Snapshot) throws {
        guard candidate.schemaVersion == 1, candidate.accounts.count <= 8,
              candidate.accounts.values.reduce(0, { $0 + $1.items.count }) <= Self.maximumItems else { throw CloudSyncError.capacity }
        for (context, account) in candidate.accounts {
            guard !context.isEmpty, context.utf8.count <= 128,
                  !(account.scanFinished && account.cursor != nil),
                  account.cursor.map({ !$0.isEmpty && $0.count <= CloudHistoryPullPage.maximumCursorBytes }) ?? true else { throw CloudSyncError.invalidData }
            var ids = Set<String>()
            for item in account.items {
                _ = try CloudHistoryItem.decode(item.payload())
                guard ids.insert(item.recordID).inserted else { throw CloudSyncError.conflict }
            }
        }
    }
}

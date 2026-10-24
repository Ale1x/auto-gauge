import Foundation

private struct CloudQueueEntry: Codable, Equatable { var item: CloudHistoryItem; var uploaded: Bool }
private struct CloudQueueSnapshot: Codable { var schemaVersion = 1; var accounts: [String: [CloudQueueEntry]] = [:] }
public struct CloudQueueStatus: Equatable, Sendable {
    public let pending: Int
    public let uploaded: Int
}

/// One actor/file per installation. Account queues never move across contexts.
public actor CloudHistoryOutbox {
    public static let maximumBytes = 512 * 1024
    public static let maximumItems = 1024
    private let storage: any LogArchiveStorage
    private var snapshot = CloudQueueSnapshot()
    private var loaded = false
    private var activeContext: String?
    private var generation: UInt64 = 0
    private var syncing = false
    public init(storage: any LogArchiveStorage) { self.storage = storage }
    public func reload() throws {
        generation &+= 1
        do {
            var candidate = CloudQueueSnapshot()
            if let data = try storage.read(maximumBytes: Self.maximumBytes) {
                guard data.count <= Self.maximumBytes else { throw CloudSyncError.capacity }
                try StrictLogJSON.check(data)
                candidate = try JSONDecoder().decode(CloudQueueSnapshot.self, from: data)
                try validate(candidate)
            }
            snapshot = candidate; loaded = true
        } catch { loaded = false; throw error }
    }
    public func selectAccount(_ context: String?) throws {
        if let context { guard !context.isEmpty, context.utf8.count <= 128 else { throw CloudSyncError.invalidData } }
        if activeContext != context { activeContext = context; generation &+= 1 }
    }
    public func invalidateAccount() { activeContext = nil; generation &+= 1 }
    public func status() -> CloudQueueStatus {
        let queue = activeContext.flatMap { snapshot.accounts[$0] } ?? []
        return .init(pending: queue.filter { !$0.uploaded }.count, uploaded: queue.filter(\.uploaded).count)
    }
    public func enqueue(_ items: [CloudHistoryItem], provenance: HistoryProvenance) throws {
        guard provenance == .realDevice else { throw CloudSyncError.simulatedData }
        guard loaded else { throw CloudSyncError.notLoaded }
        guard let context = activeContext else { throw CloudSyncError.unavailable }
        guard items.count <= Self.maximumItems else { throw CloudSyncError.capacity }
        var candidate = snapshot
        var queue = candidate.accounts[context] ?? []
        for item in items {
            _ = try CloudHistoryItem.decode(item.payload())
            if let old = queue.first(where: { $0.item.recordID == item.recordID }) {
                guard old.item == item else { throw CloudSyncError.conflict }
            } else { queue.append(.init(item: item, uploaded: false)) }
        }
        candidate.accounts[context] = queue
        try persist(candidate)
    }
    /// Explicit user action. ACK gauge is intentionally absent from this API.
    public func synchronize(database: any CloudHistoryDatabase) async throws -> CloudQueueStatus {
        guard loaded else { throw CloudSyncError.notLoaded }
        guard !syncing else { throw CloudSyncError.busy }
        guard let context = activeContext else { throw CloudSyncError.unavailable }
        syncing = true; defer { syncing = false }
        let token = generation
        try Task.checkCancellation()
        try await verifyAccount(database, context: context, token: token)
        let pending = (snapshot.accounts[context] ?? []).filter { !$0.uploaded }
        for entry in pending {
            try Task.checkCancellation()
            guard token == generation, activeContext == context else { throw CloudSyncError.accountChanged }
            try await verifyAccount(database, context: context, token: token)
            try await database.putImmutable(entry.item, context: context)
            try Task.checkCancellation()
            guard token == generation, activeContext == context else { throw CloudSyncError.accountChanged }
            try await verifyAccount(database, context: context, token: token)
            var candidate = snapshot
            guard let index = candidate.accounts[context]?.firstIndex(where: { $0.item.recordID == entry.item.recordID }) else { throw CloudSyncError.invalidData }
            candidate.accounts[context]?[index].uploaded = true
            try persist(candidate)
        }
        return status()
    }
    private func verifyAccount(_ database: any CloudHistoryDatabase, context: String, token: UInt64) async throws {
        let account = try await database.account()
        try Task.checkCancellation()
        // A stale callback may not invalidate a newer account selected during this await.
        guard token == generation, activeContext == context else { throw CloudSyncError.accountChanged }
        guard account == .available(context: context) else {
            invalidateAccount(); throw CloudSyncError.accountChanged
        }
    }
    private func persist(_ candidate: CloudQueueSnapshot) throws {
        try validate(candidate)
        let data = try JSONEncoder().encode(candidate)
        guard data.count <= Self.maximumBytes else { throw CloudSyncError.capacity }
        try storage.write(data)
        snapshot = candidate
    }
    private func validate(_ candidate: CloudQueueSnapshot) throws {
        guard candidate.schemaVersion == 1, candidate.accounts.count <= 8,
              candidate.accounts.values.reduce(0, { $0 + $1.count }) <= Self.maximumItems else { throw CloudSyncError.capacity }
        for (context, queue) in candidate.accounts {
            guard !context.isEmpty, context.utf8.count <= 128 else { throw CloudSyncError.invalidData }
            var ids = Set<String>()
            for entry in queue {
                _ = try CloudHistoryItem.decode(entry.item.payload())
                guard ids.insert(entry.item.recordID).inserted else { throw CloudSyncError.invalidData }
            }
        }
    }
}

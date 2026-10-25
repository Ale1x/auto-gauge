import Foundation

private struct QualifiedHistoryInboxSnapshot: Codable { var schemaVersion = 1; var items: [CloudHistoryItem] = [] }

/// Account-neutral local staging. Only sealed native import receipts enter here;
/// simulator records and arbitrary CloudHistoryItem arrays have no enqueue API.
public actor QualifiedHistoryInbox {
    public static let maximumBytes = 512 * 1024
    public static let maximumItems = 1024
    private let storage: any LogArchiveStorage
    private var snapshot = QualifiedHistoryInboxSnapshot()
    private var loaded = false
    private var draining = false
    private var generation: UInt64 = 0
    public init(storage: any LogArchiveStorage) { self.storage = storage }
    public func reload() throws {
        generation &+= 1
        do {
            var candidate = QualifiedHistoryInboxSnapshot()
            if let data = try storage.read(maximumBytes: Self.maximumBytes) {
                try StrictLogJSON.check(data)
                guard let fields = try JSONSerialization.jsonObject(with: data) as? [String: Any], Set(fields.keys) == ["schemaVersion", "items"] else { throw CloudSyncError.invalidData }
                candidate = try JSONDecoder().decode(QualifiedHistoryInboxSnapshot.self, from: data)
                try validate(candidate)
            }
            snapshot = candidate; loaded = true
        } catch { loaded = false; throw error }
    }
    public func enqueue(_ receipt: QualifiedHistoryImport) throws {
        if !loaded { try reload() }
        var candidate = snapshot
        for item in receipt.items {
            guard item.deviceID == receipt.deviceID, item.sessionID == receipt.sessionID else { throw CloudSyncError.invalidData }
            if let existing = candidate.items.first(where: { $0.recordID == item.recordID }) {
                guard existing == item else { throw CloudSyncError.conflict }
            } else { candidate.items.append(item) }
        }
        try persist(candidate)
    }
    public func count() -> Int { snapshot.items.count }
    /// Explicit account action. The selected account's durable outbox is committed
    /// first; a lost response/file error keeps inbox data for idempotent retry.
    public func move(to outbox: CloudHistoryOutbox) async throws {
        guard loaded else { throw CloudSyncError.notLoaded }
        guard !draining else { throw CloudSyncError.busy }
        draining = true; defer { draining = false }
        let token = generation; let pending = snapshot.items
        try Task.checkCancellation()
        try await outbox.enqueue(pending, provenance: .realDevice)
        try Task.checkCancellation()
        guard token == generation else { throw CloudSyncError.accountChanged }
        let identifiers = Set(pending.map(\.recordID))
        var candidate = snapshot; candidate.items.removeAll { identifiers.contains($0.recordID) }
        try persist(candidate)
    }
    private func persist(_ candidate: QualifiedHistoryInboxSnapshot) throws {
        try validate(candidate)
        let data = try JSONEncoder().encode(candidate)
        guard data.count <= Self.maximumBytes else { throw CloudSyncError.capacity }
        try storage.write(data); snapshot = candidate
    }
    private func validate(_ candidate: QualifiedHistoryInboxSnapshot) throws {
        guard candidate.schemaVersion == 1, candidate.items.count <= Self.maximumItems,
              Set(candidate.items.map(\.recordID)).count == candidate.items.count else { throw CloudSyncError.capacity }
        for item in candidate.items { _ = try CloudHistoryItem.decode(item.payload()) }
    }
}

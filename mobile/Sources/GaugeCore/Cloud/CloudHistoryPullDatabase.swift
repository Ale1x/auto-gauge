import Foundation

public protocol CloudHistoryPullDatabase: CloudHistoryDatabase {
    /// Cursor is opaque and account-local. A query traversal is not a consistent snapshot.
    func readHistory(context: String, cursor: Data?, limit: Int) async throws -> CloudHistoryPullPage
}

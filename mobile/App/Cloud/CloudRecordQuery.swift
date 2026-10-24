import Foundation
import CloudKit
import GaugeCore

enum CloudRecordQuery {
    static func perform(database: CKDatabase, cursor: Data?, limit: Int) async throws -> CloudHistoryPullPage {
        guard (1...CloudHistoryPullPage.maximumRecords).contains(limit) else { throw CloudSyncError.capacity }
        let operation: CKQueryOperation
        if let cursor {
            guard !cursor.isEmpty, cursor.count <= CloudHistoryPullPage.maximumCursorBytes else { throw CloudSyncError.cursorExpired }
            do {
                guard let decoded = try NSKeyedUnarchiver.unarchivedObject(ofClass: CKQueryOperation.Cursor.self, from: cursor) else { throw CloudSyncError.cursorExpired }
                operation = CKQueryOperation(cursor: decoded)
            } catch { throw CloudSyncError.cursorExpired }
        } else {
            operation = CKQueryOperation(query: CKQuery(recordType: "AGHistoryV1", predicate: NSPredicate(value: true)))
        }
        operation.resultsLimit = limit
        operation.desiredKeys = ["schemaVersion", "payload", "digest"]
        let completion = CloudQueryCompletion()
        operation.recordMatchedBlock = { _, value in completion.append(value) }
        operation.queryResultBlock = { result in completion.finish(result) }
        return try await withTaskCancellationHandler {
            try Task.checkCancellation()
            return try await withCheckedThrowingContinuation { continuation in
                completion.install(continuation); database.add(operation)
            }
        } onCancel: { operation.cancel(); completion.cancel() }
    }
}

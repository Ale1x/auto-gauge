import Foundation
import CloudKit

enum CloudRecordSave {
    static func perform(record: CKRecord, database: CKDatabase) async throws -> CKRecord {
        let operation = CKModifyRecordsOperation(recordsToSave: [record], recordIDsToDelete: [])
        operation.savePolicy = .ifServerRecordUnchanged; operation.isAtomic = false
        let completion = CloudSaveCompletion()
        operation.perRecordSaveBlock = { _, result in completion.resolve(result) }
        operation.modifyRecordsResultBlock = { result in
            if case .failure(let error) = result { completion.resolve(.failure(error)) }
        }
        return try await withTaskCancellationHandler {
            try Task.checkCancellation()
            return try await withCheckedThrowingContinuation { continuation in
                completion.install(continuation)
                database.add(operation)
            }
        } onCancel: {
            operation.cancel()
            completion.resolve(.failure(CancellationError()))
        }
    }
}

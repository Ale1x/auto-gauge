import Foundation
import CloudKit
import GaugeCore

/// Buffers one bounded page and resolves once even when cancellation races callbacks.
final class CloudQueryCompletion: @unchecked Sendable {
    private let lock = NSLock()
    private var continuation: CheckedContinuation<CloudHistoryPullPage, any Error>?
    private var result: Result<CloudHistoryPullPage, any Error>?
    private var records: [CloudHistoryRemoteRecord] = []
    private var bytes = 0
    private var failure: (any Error)?
    func install(_ value: CheckedContinuation<CloudHistoryPullPage, any Error>) {
        lock.lock()
        if let result { lock.unlock(); value.resume(with: result) }
        else { continuation = value; lock.unlock() }
    }
    func append(_ value: Result<CKRecord, any Error>) {
        lock.lock(); defer { lock.unlock() }
        guard result == nil, failure == nil else { return }
        do {
            let record = try value.get()
            guard record.recordType == "AGHistoryV1", record["schemaVersion"] as? Int == 1,
                  let payload = record["payload"] as? Data, payload.count <= 4096,
                  let digest = record["digest"] as? String, digest.utf8.count == 64,
                  records.count < CloudHistoryPullPage.maximumRecords else { throw CloudSyncError.invalidData }
            let item = CloudHistoryRemoteRecord(recordID: record.recordID.recordName, payload: payload, digest: digest)
            let cost = payload.count + item.recordID.utf8.count + digest.utf8.count
            guard bytes + cost <= CloudHistoryPullPage.maximumBytes else { throw CloudSyncError.capacity }
            _ = try item.validatedItem()
            records.append(item); bytes += cost
        } catch { failure = error }
    }
    func finish(_ value: Result<CKQueryOperation.Cursor?, any Error>) {
        lock.lock()
        guard result == nil else { lock.unlock(); return }
        let response: Result<CloudHistoryPullPage, any Error>
        do {
            if let failure { throw failure }
            let cursor = try value.get()
            let data = try cursor.map { try NSKeyedArchiver.archivedData(withRootObject: $0, requiringSecureCoding: true) }
            response = .success(try .init(records: records, nextCursor: data))
        } catch { response = .failure(error) }
        result = response
        let pending = continuation; continuation = nil
        lock.unlock(); pending?.resume(with: response)
    }
    func cancel() { finish(.failure(CancellationError())) }
}

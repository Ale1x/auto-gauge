import Foundation
import CloudKit

/// Exactly one continuation resolution, including cancellation racing a CloudKit callback.
final class CloudSaveCompletion: @unchecked Sendable {
    private let lock = NSLock()
    private var continuation: CheckedContinuation<CKRecord, any Error>?
    private var result: Result<CKRecord, any Error>?
    func install(_ value: CheckedContinuation<CKRecord, any Error>) {
        lock.lock()
        if let result { lock.unlock(); value.resume(with: result) }
        else { continuation = value; lock.unlock() }
    }
    func resolve(_ value: Result<CKRecord, any Error>) {
        lock.lock()
        guard result == nil else { lock.unlock(); return }
        result = value
        let pending = continuation; continuation = nil
        lock.unlock(); pending?.resume(with: value)
    }
}


import Foundation
@testable import GaugeCore

final class QualifiedSessionTestStorage: LogArchiveStorage, @unchecked Sendable {
    private let lock = NSLock()
    private var bytes: Data?
    private var fails = false
    func setFailure(_ value: Bool) { lock.lock(); fails = value; lock.unlock() }
    func data() -> Data? { lock.lock(); defer { lock.unlock() }; return bytes }
    func read(maximumBytes: Int) throws -> Data? {
        lock.lock(); defer { lock.unlock() }
        guard (bytes?.count ?? 0) <= maximumBytes else { throw LogImportError.capacity }; return bytes
    }
    func write(_ data: Data) throws {
        lock.lock(); defer { lock.unlock() }
        guard !fails else { throw LogImportError.capacity }; bytes = data
    }
}

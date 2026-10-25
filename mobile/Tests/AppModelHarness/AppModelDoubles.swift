import Foundation
import GaugeCore

/// Explicit test boundaries: production AppModel and GaugeCore remain unchanged.
@MainActor final class NativeBluetoothController {
    static var packetRequests = 0
    static var nextPipe: (any GaugePacketPipe)?
    var onSessionInvalidated: (@MainActor (UInt64) -> Void)?
    func qualifiedPacketPipe() async throws -> any GaugePacketPipe { Self.packetRequests += 1; guard let pipe = Self.nextPipe else { throw GaugeLinkError.unqualified }; return pipe }
    func isCurrentConnection(_ generation: UInt64) -> Bool { Self.nextPipe != nil }
    func stop() {}
}
@MainActor final class CloudAccountModel {
    var calls = 0
    var configured = false
    var busy = false
    var account: CloudAccount = .notConfigured
    var notice = "Non disponibile"
    var lastOperationSucceeded = false
    var lastSynchronizationDate: Date?
    var restoreFinished = false
    func verify() { calls += 1 }
    func synchronize() { calls += 1; lastOperationSucceeded = true; lastSynchronizationDate = .now; notice = "Copia di sicurezza aggiornata" }
    func restore(restart: Bool = false) { calls += 1; lastOperationSucceeded = true; notice = "Viaggi recuperati" }
    func cancel() { calls += 1 }
    func recoverQualifiedImports(from archive: LocalLogArchive) async throws { calls += 1 }
    func queueQualifiedImport(_ receipt: QualifiedHistoryImport) async throws { calls += 1 }
}

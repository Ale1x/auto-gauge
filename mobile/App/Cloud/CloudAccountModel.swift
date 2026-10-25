import SwiftUI
import Observation
import CloudKit
import GaugeCore

@MainActor @Observable
final class CloudAccountModel {
    var account: CloudAccount = .notConfigured
    var notice = "iCloud non configurato per questa installazione."
    private(set) var diagnosticDetail: String?
    var busy = false
    private(set) var lastOperationSucceeded = false
    private(set) var lastSynchronizationDate: Date?
    var pending = 0
    var uploaded = 0
    var restoredItems: [CloudHistoryItem] = []
    var restoreFinished = false
    var restoreHasCheckpoint = false
    private let database: (any CloudHistoryPullDatabase)?
    private let outbox: CloudHistoryOutbox
    private let qualifiedInbox: QualifiedHistoryInbox
    private let restoreStore: CloudHistoryRestoreStore
    private var operation: Task<Void, Never>?
    private var epoch = 0
    init() {
        let directory = URL.applicationSupportDirectory.appending(path: "AutoGauge")
        qualifiedInbox = QualifiedHistoryInbox(storage: AtomicLogFileStorage(fileURL: directory.appending(path: "qualified-native-cloud-inbox-v1.json")))
        outbox = CloudHistoryOutbox(storage: AtomicLogFileStorage(fileURL: directory.appending(path: "cloud-outbox-v1.json")))
        restoreStore = CloudHistoryRestoreStore(storage: AtomicLogFileStorage(fileURL: directory.appending(path: "cloud-restored-v1.json")))
        database = CloudDevelopmentConfiguration.enabled ? NativeCloudHistoryDatabase(containerIdentifier: CloudDevelopmentConfiguration.container) : nil
    }
    var configured: Bool { database != nil }
    var accountTitle: String {
        switch account {
        case .notConfigured: return "iCloud non configurato"
        case .available: return "Account iCloud disponibile"
        case .noAccount: return "Accedi a iCloud nelle Impostazioni"
        case .restricted: return "Accesso iCloud limitato"
        case .temporarilyUnavailable: return "iCloud temporaneamente non disponibile"
        }
    }
    func verify() {
        guard let database, !busy else { return }
        busy = true; diagnosticDetail = nil; lastOperationSucceeded = false; epoch += 1; let token = epoch
        operation = Task {
            do {
                let result = try await database.account()
                try Task.checkCancellation()
                guard token == epoch else { return }
                try await outbox.reload()
                try await qualifiedInbox.reload()
                try Task.checkCancellation()
                guard token == epoch else { return }
                try await restoreStore.reload()
                try Task.checkCancellation()
                guard token == epoch else { return }
                let context: String?
                if case .available(let value) = result { context = value } else { context = nil }
                try await outbox.selectAccount(context)
                guard token == epoch else { return }
                try await restoreStore.selectAccount(context)
                guard token == epoch else { return }
                clearProjection()
                account = result
                let status = await outbox.status()
                guard token == epoch else { return }
                pending = status.pending; uploaded = status.uploaded
                let restored = await restoreStore.progress()
                guard token == epoch else { return }
                show(restored)
                lastOperationSucceeded = true
                notice = "Account verificato. Puoi aggiornare la copia di sicurezza."
            } catch {
                guard token == epoch else { return }
                diagnosticDetail = String(describing: error)
                clearProjection()
                account = .temporarilyUnavailable; notice = "Non è stato possibile aggiornare iCloud. I dati già salvati sono conservati. Riprova."
            }
            guard token == epoch else { return }
            busy = false
        }
    }
    /// Local recovery is account-neutral and does not contact CloudKit.
    func recoverQualifiedImports(from archive: LocalLogArchive) async throws {
        try await archive.recoverQualifiedImports(to: qualifiedInbox)
    }
    /// Import only sealed qualified-device receipts; no arbitrary app logSessions.
    func queueQualifiedImport(_ receipt: QualifiedHistoryImport) async throws {
        try await qualifiedInbox.enqueue(receipt)
    }
    /// Explicit account action. Simulator histories have no inbox enqueue path.
    func synchronize() {
        guard let database, !busy, case .available = account else { return }
        busy = true; diagnosticDetail = nil; lastOperationSucceeded = false; epoch += 1; let token = epoch
        operation = Task {
            do {
                try await qualifiedInbox.move(to: outbox)
                let status = try await outbox.synchronize(database: database)
                guard token == epoch else { return }
                pending = status.pending; uploaded = status.uploaded
                lastOperationSucceeded = true; lastSynchronizationDate = .now
                notice = status.uploaded > 0 ? "Copia di sicurezza aggiornata." : "Non ci sono nuovi viaggi da salvare."
            } catch {
                guard token == epoch else { return }
                diagnosticDetail = String(describing: error)
                notice = "Non è stato possibile aggiornare iCloud. I dati già salvati sono conservati. Riprova."
            }
            guard token == epoch else { return }
            busy = false
        }
    }
    func restore(restart: Bool = false) {
        guard let database, !busy, case .available = account else { return }
        busy = true; diagnosticDetail = nil; lastOperationSucceeded = false; epoch += 1; let token = epoch
        operation = Task {
            do {
                if restart {
                    try await restoreStore.restartScan()
                    try Task.checkCancellation()
                    guard token == epoch else { return }
                }
                let progress = try await restoreStore.restore(database: database)
                guard token == epoch else { return }
                show(progress); lastOperationSucceeded = true
                notice = progress.scanFinished ? "Ricerca completata. Puoi controllare nuovamente se mancano viaggi recenti." : "Alcuni viaggi sono stati recuperati. Premi Recupera viaggi per continuare."
            } catch {
                guard token == epoch else { return }
                diagnosticDetail = String(describing: error)
                let progress = await restoreStore.progress()
                guard token == epoch else { return }
                show(progress); notice = "Non è stato possibile aggiornare iCloud. I dati già salvati sono conservati. Riprova."
            }
            guard token == epoch else { return }
            busy = false
        }
    }
    private func clearProjection() {
        pending = 0; uploaded = 0
        restoredItems = []; restoreFinished = false; restoreHasCheckpoint = false
    }
    private func show(_ progress: CloudRestoreProgress) {
        restoredItems = progress.items; restoreFinished = progress.scanFinished; restoreHasCheckpoint = progress.hasCheckpoint
    }
    func cancel() {
        epoch += 1; operation?.cancel(); operation = nil; busy = false; lastOperationSucceeded = false
        notice = "Operazione annullata. I viaggi salvati sono conservati."
    }
    func accountChanged() async {
        epoch += 1; let token = epoch
        operation?.cancel(); operation = nil; busy = false
        clearProjection(); lastSynchronizationDate = nil; lastOperationSucceeded = false
        account = configured ? .temporarilyUnavailable : .notConfigured
        notice = "Il tuo account iCloud è cambiato. Premi Sincronizza per verificarlo."
        await outbox.invalidateAccount()
        guard token == epoch else { return }
        await restoreStore.invalidateAccount()
        guard token == epoch else { return }
    }
}

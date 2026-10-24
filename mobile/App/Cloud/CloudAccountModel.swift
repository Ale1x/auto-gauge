import SwiftUI
import Observation
import CloudKit
import GaugeCore

@MainActor @Observable
final class CloudAccountModel {
    var account: CloudAccount = .notConfigured
    var notice = "iCloud non configurato per questa installazione."
    var busy = false
    var pending = 0
    var uploaded = 0
    var restoredItems: [CloudHistoryItem] = []
    var restoreFinished = false
    var restoreHasCheckpoint = false
    private let database: (any CloudHistoryPullDatabase)?
    private let outbox: CloudHistoryOutbox
    private let restoreStore: CloudHistoryRestoreStore
    private var operation: Task<Void, Never>?
    private var epoch = 0
    init() {
        let directory = URL.applicationSupportDirectory.appending(path: "AutoGauge")
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
        busy = true; epoch += 1; let token = epoch
        operation = Task {
            do {
                let result = try await database.account()
                try Task.checkCancellation()
                guard token == epoch else { return }
                try await outbox.reload()
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
                notice = "Account verificato. Nessun dato è stato caricato."
            } catch {
                guard token == epoch else { return }
                clearProjection()
                account = .temporarilyUnavailable; notice = error.localizedDescription
            }
            guard token == epoch else { return }
            busy = false
        }
    }
    /// This app has only simulated histories: never enqueue its logSessions here.
    func synchronize() {
        guard let database, !busy, case .available = account else { return }
        busy = true; epoch += 1; let token = epoch
        operation = Task {
            do {
                let status = try await outbox.synchronize(database: database)
                guard token == epoch else { return }
                pending = status.pending; uploaded = status.uploaded
                notice = status.uploaded > 0 ? "Coda reale confermata su iCloud: \(status.uploaded) elementi. Gli storici simulati sono esclusi." : "Nessuno storico reale da sincronizzare. I dati simulati restano sul telefono."
            } catch {
                guard token == epoch else { return }
                notice = error.localizedDescription
            }
            guard token == epoch else { return }
            busy = false
        }
    }
    func restore(restart: Bool = false) {
        guard let database, !busy, case .available = account else { return }
        busy = true; epoch += 1; let token = epoch
        operation = Task {
            do {
                if restart {
                    try await restoreStore.restartScan()
                    try Task.checkCancellation()
                    guard token == epoch else { return }
                }
                let progress = try await restoreStore.restore(database: database)
                guard token == epoch else { return }
                show(progress)
                notice = progress.scanFinished ? "Scansione iCloud terminata. L’indice può aggiornarsi in ritardo: non è una fotografia completa del cloud. Puoi avviare una nuova scansione." : "Recupero parziale salvato. Continua dal checkpoint per leggere altre pagine."
            } catch {
                guard token == epoch else { return }
                let progress = await restoreStore.progress()
                guard token == epoch else { return }
                show(progress); notice = error.localizedDescription
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
        epoch += 1; operation?.cancel(); operation = nil; busy = false
        notice = "Operazione annullata. La coda locale resta conservata; una risposta cloud persa verrà verificata al prossimo tentativo."
    }
    func accountChanged() async {
        epoch += 1; let token = epoch
        operation?.cancel(); operation = nil; busy = false
        clearProjection()
        account = configured ? .temporarilyUnavailable : .notConfigured
        notice = "Account cambiato. Verifica nuovamente iCloud; le code di altri account restano separate."
        await outbox.invalidateAccount()
        guard token == epoch else { return }
        await restoreStore.invalidateAccount()
        guard token == epoch else { return }
    }
}

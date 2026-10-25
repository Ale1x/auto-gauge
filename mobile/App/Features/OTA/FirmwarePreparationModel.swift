import Foundation
import Observation
import GaugeCore

@MainActor @Observable
final class FirmwarePreparationModel {
    private(set) var snapshot = OTAPreparationSnapshot()
    private(set) var busy = false
    private(set) var notice: String?
    let trustAvailable: Bool
    @ObservationIgnored private let trust: OTAPreparationTrust?
    @ObservationIgnored private let preparation: OTAPackagePreparation
    @ObservationIgnored private var work: Task<Void, Never>?
    private var generation = 0
    init(trust: OTAPreparationTrust? = nil) {
        self.trust = trust; trustAvailable = trust != nil
        let file = URL.applicationSupportDirectory.appending(path: "AutoGauge/prepared-ota-v1.agop")
        preparation = OTAPackagePreparation(archiveURL: file)
    }
    var canVerify: Bool { trustAvailable && snapshot.manifestFile != nil && snapshot.imageFile != nil && !busy }
    var canSave: Bool { snapshot.verifiedManifest != nil && !busy }
    func importFile(_ url: URL, kind: OTAPreparationFileKind) {
        run(success: "File copiato nella preparazione locale. Verifica nuovamente il pacchetto.") { preparation in
            try await preparation.importFile(url, kind: kind)
        }
    }
    func verify() {
        let trust = self.trust
        run(success: "Firma, compatibilità e impronta del pacchetto verificate offline. Nessuna installazione eseguita.") { preparation in
            try await preparation.verify(trust: trust)
        }
    }
    func save() {
        run(success: "Preparazione salvata sul telefono. Alla riapertura sarà verificata di nuovo.") { preparation in
            try await preparation.save(); return await preparation.snapshot()
        }
    }
    func restore() async {
        guard !busy else { return }
        generation += 1; let token = generation; busy = true
        let task = Task {
            do {
                let restored = try await preparation.restore(trust: trust)
                try Task.checkCancellation()
                guard token == generation else { return }
                snapshot = await preparation.snapshot()
                guard token == generation else { return }
                if restored { notice = "Preparazione locale riaperta e verificata con il profilo fidato corrente." }
            } catch {
                guard token == generation else { return }
                snapshot = await preparation.snapshot()
                guard token == generation else { return }
                notice = "Preparazione locale non utilizzabile: \(error.localizedDescription) Il salvataggio è conservato."
            }
            if token == generation { busy = false; work = nil }
        }
        work = task
        await withTaskCancellationHandler { await task.value } onCancel: { task.cancel() }
    }
    func cancel() {
        generation += 1; let token = generation
        let previous = work
        previous?.cancel(); busy = true
        snapshot = .init(manifestFile: snapshot.manifestFile, imageFile: snapshot.imageFile)
        notice = "Operazione interrotta. Verifica nuovamente i file prima di usarli. Un salvataggio già avvenuto sarà ricontrollato alla riapertura."
        work = Task {
            // Await the previous operation, including prior cleanup, before
            // invalidating: actor scheduling never lets stale cleanup run last.
            if let previous { await previous.value }
            guard token == generation, !Task.isCancelled else { return }
            await preparation.invalidateVerification()
            guard token == generation else { return }
            busy = false; work = nil
        }
    }
    func importFailed(_ error: Error) {
        let value = error as NSError
        guard !(value.domain == NSCocoaErrorDomain && value.code == NSUserCancelledError) else { return }
        notice = "Impossibile selezionare il file: \(error.localizedDescription)"
    }
    private func run(success: String, operation: @escaping @Sendable (OTAPackagePreparation) async throws -> OTAPreparationSnapshot) {
        guard !busy else { return }
        generation += 1; let token = generation
        busy = true; notice = nil
        snapshot = .init(manifestFile: snapshot.manifestFile, imageFile: snapshot.imageFile)
        let preparation = self.preparation
        work = Task {
            do {
                let next = try await operation(preparation)
                try Task.checkCancellation()
                guard token == generation else { return }
                snapshot = next; notice = success
            } catch {
                guard token == generation else { return }
                snapshot = await preparation.snapshot()
                guard token == generation else { return }
                notice = "Operazione non completata: \(error.localizedDescription)"
            }
            if token == generation { busy = false; work = nil }
        }
    }
}

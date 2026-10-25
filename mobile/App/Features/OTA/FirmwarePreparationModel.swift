import Foundation
import Observation
import GaugeCore

@MainActor @Observable
final class FirmwarePreparationModel {
    private(set) var snapshot = OTAPreparationSnapshot()
    private(set) var busy = false
    private(set) var notice: String?
    private(set) var installationSnapshot = OTAInstallationSnapshot()
    let installationAvailable: Bool
    @ObservationIgnored private let installation: OTAInstallationController
    let trustAvailable: Bool
    @ObservationIgnored private let trust: OTAPreparationTrust?
    @ObservationIgnored private let preparation: OTAPackagePreparation
    @ObservationIgnored private var work: Task<Void, Never>?
    private var generation = 0
    init(trust: OTAPreparationTrust? = nil, installation: OTAInstallationController = .init()) {
        self.trust = trust; trustAvailable = trust != nil
        self.installation = installation; installationAvailable = installation.configured
        let file = URL.applicationSupportDirectory.appending(path: "AutoGauge/prepared-ota-v1.agop")
        preparation = OTAPackagePreparation(archiveURL: file)
    }
    var preparationLocked: Bool { busy || installationSnapshot.state != .idle }
    var canVerify: Bool { trustAvailable && snapshot.manifestFile != nil && snapshot.imageFile != nil && !preparationLocked }
    var canUpload: Bool { installationAvailable && snapshot.verifiedManifest != nil && !preparationLocked }
    var canSelect: Bool { installationSnapshot.state == .ready && !busy }
    var canSave: Bool { snapshot.verifiedManifest != nil && !preparationLocked }
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
        let current = await installation.snapshot()
        guard token == generation else { return }
        installationSnapshot = current
        guard current.state == .idle else { busy = false; return }
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
    func upload() {
        guard canUpload else { return }
        performInstallation(selecting: false)
    }
    func selectInstalledFirmware() {
        guard canSelect else { return }
        performInstallation(selecting: true)
    }
    private func performInstallation(selecting: Bool) {
        generation += 1; let token = generation
        busy = true; notice = nil
        let preparation = self.preparation; let installation = self.installation
        installationSnapshot = .init(manifest: selecting ? installationSnapshot.manifest : snapshot.verifiedManifest,
                                     state: selecting ? .selecting : .opening, progress: installationSnapshot.progress,
                                     selectionMayHaveChanged: selecting)
        work = Task { [self] in
            do {
                if selecting { try await installation.select() }
                else {
                    let candidate = try await preparation.verifiedCandidate()
                    let id = UInt32.random(in: 1...UInt32.max)
                    try await installation.upload(candidate, transferID: id) { [weak self] value in
                        await self?.receiveProgress(value, token: token)
                    }
                }
                guard token == generation else { return }
                installationSnapshot = await installation.snapshot()
                guard token == generation else { return }
                notice = selecting ? "Selezione confermata dal gauge. Avvio e salute del nuovo firmware devono ancora essere verificati sul dispositivo." : "Trasferimento verificato dal gauge. Il firmware è pronto; selezionalo esplicitamente per il prossimo avvio."
            } catch {
                guard token == generation else { return }
                installationSnapshot = await installation.snapshot()
                guard token == generation else { return }
                notice = installationSnapshot.state == .unknown ? "Esito non determinabile. Nessun nuovo invio o selezione automatica: riconcilia lo stato del gauge prima di un altro aggiornamento." : "Installazione non avviata: \(error.localizedDescription)"
            }
            if token == generation { busy = false; work = nil }
        }
    }
    private func receiveProgress(_ value: OTAInstallationProgress, token: Int) {
        guard token == generation else { return }
        installationSnapshot = .init(manifest: installationSnapshot.manifest, state: .uploading, progress: value)
    }
    func cancelInstallation() {
        generation += 1; let token = generation
        let previous = work; previous?.cancel(); busy = true
        work = Task {
            await installation.cancel()
            if let previous { await previous.value }
            guard token == generation else { return }
            installationSnapshot = await installation.snapshot()
            guard token == generation else { return }
            notice = "Operazione interrotta e collegamento chiuso. I byte già ricevuti non vengono annullati; nessun nuovo tentativo automatico."
            busy = false; work = nil
        }
    }
    func leave() {
        if installationSnapshot.state == .opening || installationSnapshot.state == .uploading || installationSnapshot.state == .ready || installationSnapshot.state == .selecting {
            cancelInstallation()
        } else if busy { cancel() }
    }
    func importFailed(_ error: Error) {
        let value = error as NSError
        guard !(value.domain == NSCocoaErrorDomain && value.code == NSUserCancelledError) else { return }
        notice = "Impossibile selezionare il file: \(error.localizedDescription)"
    }
    private func run(success: String, operation: @escaping @Sendable (OTAPackagePreparation) async throws -> OTAPreparationSnapshot) {
        guard !preparationLocked else { return }
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

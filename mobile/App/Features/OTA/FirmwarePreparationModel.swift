import Foundation
import Observation
import GaugeCore

@MainActor @Observable
final class FirmwarePreparationModel {
    private(set) var snapshot = OTAPreparationSnapshot()
    private(set) var busy = false
    private(set) var notice: String?
    private(set) var diagnosticDetail: String?
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
        run(success: "File aggiunto. Verifica entrambi i file prima di continuare.") { preparation in
            try await preparation.importFile(url, kind: kind)
        }
    }
    func verify() {
        let trust = self.trust
        run(success: "I file sono stati verificati. L’aggiornamento non è ancora stato installato.") { preparation in
            try await preparation.verify(trust: trust)
        }
    }
    func save() {
        run(success: "File conservati sul telefono. Verranno ricontrollati quando li riaprirai.") { preparation in
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
                if restored { notice = "File salvati riaperti e verificati." }
            } catch {
                guard token == generation else { return }
                snapshot = await preparation.snapshot()
                guard token == generation else { return }
                recordDiagnostic(error)
                notice = "Non è stato possibile riaprire l’aggiornamento salvato. I file restano conservati sul telefono."
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
        notice = "Controllo interrotto. Verifica nuovamente i file prima di usarli. Gli eventuali file già salvati restano sul telefono."
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
        busy = true; notice = nil; diagnosticDetail = nil
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
                notice = selecting ? "Versione scelta per il prossimo avvio. Il corretto avvio deve ancora essere confermato sul dispositivo." : "File trasferito e controllato dal dispositivo. Premi Usa al prossimo avvio per scegliere la nuova versione."
            } catch {
                guard token == generation else { return }
                installationSnapshot = await installation.snapshot()
                guard token == generation else { return }
                recordDiagnostic(error)
                notice = installationSnapshot.state == .unknown ? "L’esito non è confermato. Controlla lo stato del dispositivo prima di ripetere l’aggiornamento." : "Non è stato possibile avviare l’aggiornamento. Controlla il collegamento al dispositivo."
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
            notice = "Collegamento interrotto. Il dispositivo potrebbe aver già ricevuto parte dell’aggiornamento: controllane lo stato prima di riprovare."
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
        recordDiagnostic(error)
        notice = "Non è stato possibile aprire il file. Prova a selezionarlo di nuovo."
    }
    private func run(success: String, operation: @escaping @Sendable (OTAPackagePreparation) async throws -> OTAPreparationSnapshot) {
        guard !preparationLocked else { return }
        generation += 1; let token = generation
        busy = true; notice = nil; diagnosticDetail = nil
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
                recordDiagnostic(error)
                notice = preparationNotice(for: error)
            }
            if token == generation { busy = false; work = nil }
        }
    }
    private func recordDiagnostic(_ error: Error) {
        diagnosticDetail = String(reflecting: error)
    }
    private func preparationNotice(for error: Error) -> String {
        if error is CancellationError { return "Controllo interrotto." }
        if let preparationError = error as? OTAPreparationError {
            switch preparationError {
            case .trustUnavailable: return "La verifica non è ancora disponibile per questo dispositivo."
            case .missingFiles: return "Scegli le informazioni e il file dell’aggiornamento prima di continuare."
            case .invalidFile: return "Non è stato possibile aprire il file. Prova a selezionarlo di nuovo."
            case .fileTooLarge: return "Il file è vuoto o troppo grande. Scegli il file fornito per il dispositivo."
            case .invalidArchive: return "Il salvataggio non è utilizzabile. Scegli nuovamente i file dell’aggiornamento."
            case .notVerified: return "Premi Verifica aggiornamento prima di continuare."
            }
        }
        if let packageError = error as? OTAPackageError {
            switch packageError {
            case .hardware, .profile: return "Questo aggiornamento non è adatto al tuo dispositivo."
            case .replay: return "Questa versione non può essere installata. Scegli un aggiornamento più recente."
            case .capacity: return "Questo aggiornamento è troppo grande per il dispositivo."
            case .untrustedKey, .publicKey, .signature:
                return "Non è possibile confermare l’origine dell’aggiornamento. Usa i file ufficiali forniti per il dispositivo."
            case .image: return "I due file non corrispondono. Scegli le informazioni e il file dello stesso aggiornamento."
            case .envelope, .schema, .manifest:
                return "Le informazioni dell’aggiornamento non sono valide. Scegli nuovamente i file forniti per il dispositivo."
            }
        }
        return "L’operazione non è stata completata. I file già salvati restano sul telefono."
    }

}

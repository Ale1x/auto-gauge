import SwiftUI
import CloudKit

struct AccountView: View {
    @Environment(AppModel.self) private var model
    private var cloud: CloudAccountModel { model.cloud }
    var body: some View {
        Form {
            Section {
                Label("Account iCloud del telefono", systemImage: "icloud").font(.headline)
                Text("La sincronizzazione userà CloudKit con l’account iCloud già presente sul telefono, senza un account email separato.")
                Label(cloud.accountTitle, systemImage: cloud.configured ? "icloud" : "icloud.slash").foregroundStyle(.secondary)
                Text(cloud.notice)
                    .font(.subheadline).foregroundStyle(.secondary)
            }
            Section {
                AppActionButton(title: "Verifica account iCloud", symbol: "person.crop.circle", disabled: !cloud.configured || cloud.busy, action: cloud.verify)
                AppActionButton(title: "Sincronizza storici reali", symbol: "arrow.triangle.2.circlepath.icloud", disabled: !cloud.configured || cloud.busy || !accountAvailable, action: cloud.synchronize)
                if cloud.busy {
                    ProgressView("Operazione iCloud in corso")
                    AppActionButton(title: "Annulla operazione iCloud", symbol: "xmark.circle", action: cloud.cancel)
                }
                Text("Coda di questo account: \(cloud.pending) in attesa · \(cloud.uploaded) confermati").font(.subheadline)
            } footer: { Text("Le fixture restano sul telefono anche con iCloud attivo. L’ACK del gauge conferma il salvataggio locale, non la copia cloud. La sincronizzazione richiede un’azione esplicita.") }
            Section("Collegamento al gauge") {
                Label("Più telefoni · una connessione attiva", systemImage: "iphone.radiowaves.left.and.right")
                Text("Più telefoni possono essere autorizzati e usati in momenti diversi. Cambiare o aggiungere telefono non richiede reset. L’autenticazione della sessione resta distinta dall’account iCloud.")
            }
            Section("Manutenzione fisica separata") {
                Text("Il reset volontario richiede USB e conferma fisica qualificata. Cancellerà credenziali, configurazione e log locali del gauge; non eliminerà dati cloud o storici già sincronizzati. Non serve per cambiare telefono.")
                Text("Questa app non esegue reset reali.").font(.footnote).foregroundStyle(.secondary)
            }
        }.navigationTitle("Account")

    }
    private var accountAvailable: Bool {
        if case .available = cloud.account { return true }; return false
    }
}

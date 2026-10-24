import SwiftUI
import GaugeCore

struct FirmwareView: View {
    var body: some View {
        Form {
            Section("Dispositivo dimostrativo") {
                LabeledContent("Firmware", value: "Non dichiarato dal simulatore")
                LabeledContent("Aggiornamento", value: "Non disponibile")
            }
            Section {
                AppActionButton(title: "Verifica aggiornamenti", symbol: "arrow.down.circle", disabled: true, action: {})
            } footer: { Text("Gli aggiornamenti BLE saranno abilitati dopo verifica di firma, compatibilità hardware e procedura di recupero. Nessun file firmware viene scaricato o installato da questa versione.") }
        }.navigationTitle("Firmware")
    }
}

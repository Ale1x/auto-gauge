import SwiftUI

/// The same content is used in the screen's ScrollView and in static design renders.
struct DeviceContentView: View {
    var body: some View {
        VStack(alignment: .leading, spacing: AppTheme.pageSpacing) {
            VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
                Text("IL TUO GAUGE").font(.subheadline).foregroundStyle(.secondary)
                Text("Configurazioni pronte per il viaggio.").font(.largeTitle).bold().fixedSize(horizontal: false, vertical: true)
                Text("Prepara il display e consulta gli storici. Il gauge funzionerà autonomamente in auto.").foregroundStyle(.secondary)
            }
            DeviceSummaryCard()
            SimulatedGaugeCard()
            VStack(spacing: 0) {
                NavigationLink(value: DeviceRoute.brightness) { DeviceSettingsRow(title: "Luminosità", detail: "Manuale o sensore ambiente", symbol: "sun.max") }
                Divider()
                NavigationLink(value: DeviceRoute.catalog) { DeviceSettingsRow(title: "Catalogo misure", detail: "Capability del simulatore", symbol: "list.bullet.rectangle") }
                Divider()
                NavigationLink(value: DeviceRoute.firmware) { DeviceSettingsRow(title: "Firmware", detail: "OTA reale non disponibile", symbol: "arrow.down.circle") }
            }.modifier(AppCardStyle())
            Text("Più telefoni potranno essere autorizzati, con una sola connessione Bluetooth attiva alla volta. Il collegamento e l’autenticazione reali sono ancora da integrare.").font(.footnote).foregroundStyle(.secondary)
        }
        .padding(AppTheme.pageSpacing).frame(maxWidth: AppTheme.readableWidth).frame(maxWidth: .infinity)
    }
}

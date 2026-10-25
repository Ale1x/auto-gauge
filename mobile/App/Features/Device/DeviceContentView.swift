import SwiftUI

struct DeviceContentView: View {
    var body: some View {
        VStack(alignment: .leading, spacing: AppTheme.pageSpacing) {
            VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
                Text("Il tuo display").font(.largeTitle.bold())
                Text("Le informazioni della tua auto, in un posto solo.").font(.subheadline).foregroundStyle(.secondary)
            }
            DeviceSummaryCard()
            VStack(alignment: .leading, spacing: AppTheme.spacing) {
                Text("Il dispositivo").font(.headline)
                VStack(spacing: 0) {
                    NavigationLink(value: DeviceRoute.brightness) { DeviceSettingsRow(title: "Luminosità", detail: "La luce giusta per ogni momento", symbol: "sun.max") }
                    Divider()
                    NavigationLink(value: DeviceRoute.catalog) { DeviceSettingsRow(title: "Misure disponibili", detail: "Le informazioni della tua auto", symbol: "gauge.with.needle") }
                    Divider()
                    NavigationLink(value: DeviceRoute.firmware) { DeviceSettingsRow(title: "Aggiornamenti", detail: "Controlla le novità per il display", symbol: "arrow.down.circle") }
                }.modifier(AppCardStyle())
            }
        }
        .padding(AppTheme.pageSpacing).frame(maxWidth: AppTheme.readableWidth).frame(maxWidth: .infinity)
    }
}

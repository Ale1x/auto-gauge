import SwiftUI

struct DeviceContentView: View {
    @Environment(\.dynamicTypeSize) private var dynamicTypeSize
    var body: some View {
        VStack(alignment: .leading, spacing: AppTheme.pageSpacing) {
            VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
                if dynamicTypeSize.isAccessibilitySize {
                    Text("Il tuo display").font(.title2).bold()
                } else {
                    Text("IL TUO DISPLAY").font(.subheadline).foregroundStyle(.secondary)
                    Text("Ogni viaggio, a modo tuo.").font(.largeTitle).bold().fixedSize(horizontal: false, vertical: true)
                    Text("Scegli le misure e i colori del tuo display.").foregroundStyle(.secondary)
                }
            }
            DeviceSummaryCard()
            SimulatedGaugeCard()
            VStack(spacing: 0) {
                NavigationLink(value: DeviceRoute.brightness) { DeviceSettingsRow(title: "Luminosità", detail: "Il comfort giusto, di giorno e di notte", symbol: "sun.max") }
                Divider()
                NavigationLink(value: DeviceRoute.catalog) { DeviceSettingsRow(title: "Misure disponibili", detail: "Scegli le informazioni che ti interessano", symbol: "list.bullet.rectangle") }
                Divider()
                NavigationLink(value: DeviceRoute.firmware) { DeviceSettingsRow(title: "Aggiornamenti", detail: "Mantieni il dispositivo aggiornato", symbol: "arrow.down.circle") }
            }.modifier(AppCardStyle())
        }
        .padding(AppTheme.pageSpacing).frame(maxWidth: AppTheme.readableWidth).frame(maxWidth: .infinity)
    }
}

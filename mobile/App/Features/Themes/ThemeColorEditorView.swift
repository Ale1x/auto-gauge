import SwiftUI
import GaugeCore

struct ThemeColorEditorView: View {
    @Environment(AppModel.self) private var model
    @Environment(\.self) private var environment
    @State private var accent = Color.cyan
    @State private var secondary = Color.blue
    private func hex(_ color: Color) -> UInt32 {
        let value = color.resolve(in: environment)
        func channel(_ number: Float) -> UInt32 { UInt32((min(1, max(0, number)) * 255).rounded()) }
        return (channel(value.red) << 16) | (channel(value.green) << 8) | channel(value.blue)
    }
    private var preview: GaugeTheme {
        GaugeTheme(id: "custom", title: "Il mio tema", brand: "Personale", dialTitle: "AUTO GAUGE", summary: "", accent: hex(accent), secondary: hex(secondary), stripes: [hex(accent), hex(secondary), 0xf2f5fa])
    }
    var body: some View {
        Form {
            Section {
                GaugePreview(dashboard: model.configuration.dashboards[0], theme: preview)
                    .frame(maxWidth: .infinity).padding(.vertical, AppTheme.spacing)
            }.listRowBackground(Color.clear)
            Section("I tuoi colori") {
                ColorPicker("Colore principale", selection: $accent, supportsOpacity: false)
                ColorPicker("Colore secondario", selection: $secondary, supportsOpacity: false)
            }
            Section {
                Button("Usa i miei colori", systemImage: "checkmark") {
                    model.themes.saveCustom(accent: hex(accent), secondary: hex(secondary), demo: model.isDemoMode)
                }
                .frame(minHeight: AppTheme.minimumTouch)
                .accessibilityIdentifier("theme.custom.save")
                if model.displayTheme.id == "custom" {
                    Label("Il tuo tema è in uso", systemImage: "checkmark.circle").foregroundStyle(AppTheme.accent)
                }
            } footer: { Text("Creare il tuo tema è sempre gratuito. La disposizione delle misure si modifica in Schermate.") }
        }
        .navigationTitle("Crea il tuo tema").navigationBarTitleDisplayMode(.inline)
        .onAppear {
            accent = Color(gaugeHex: model.displayTheme.accent)
            secondary = Color(gaugeHex: model.displayTheme.secondary)
        }
    }
}

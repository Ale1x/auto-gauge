import SwiftUI
import GaugeCore

struct BrightnessView: View {
    @Environment(AppModel.self) private var model
    @Environment(\.dynamicTypeSize) private var typeSize
    var body: some View {
        @Bindable var model = model
        Group {
            if !model.isDemoMode && !model.isNativeSession {
                ContentUnavailableView("Collega il dispositivo", systemImage: "link", description: Text("Collega Auto Gauge per regolare la luce del display."))
            } else {
                Form {
                    Section {
                        Label("La luce del tuo display", systemImage: "sun.max.fill").font(.headline)
                        Text("Scegli una regolazione automatica o imposta il livello che preferisci.").font(.subheadline).foregroundStyle(.secondary)
                    }
                    Section {
                        if typeSize.isAccessibilitySize { modePicker.pickerStyle(.menu) }
                        else { modePicker.pickerStyle(.segmented) }
                    } header: { Text("Regolazione") } footer: {
                        Text(model.configuration.brightness.mode == .automatic ? "Il display si adatta alla luce dell’ambiente." : "Questa impostazione cambia il display di Auto Gauge.")
                    }
                    if model.configuration.brightness.mode == .manual {
                        Section {
                            LabeledContent("Intensità") { Text("\(model.configuration.brightness.manualPercent)%").font(.title2.bold()).monospacedDigit() }
                            Slider(value: $model.manualBrightness, in: 0...100, step: 1) {
                                Text("Luminosità del display")
                            } minimumValueLabel: { Image(systemName: "sun.min") } maximumValueLabel: { Image(systemName: "sun.max.fill") }
                                .frame(minHeight: AppTheme.minimumTouch)
                                .accessibilityValue("\(model.configuration.brightness.manualPercent) per cento")
                        }
                    }
                    DraftActionsView()
                }.disabled(model.isSaving)
            }
        }
        .navigationTitle("Luminosità").navigationBarTitleDisplayMode(.inline)
    }
    private var modePicker: some View {
        @Bindable var model = model
        return Picker("Luminosità", selection: $model.configuration.brightness.mode) {
            ForEach(BrightnessMode.allCases, id: \.self) { Text($0.nameIT).tag($0) }
        }.accessibilityIdentifier("settings.brightness.mode")
    }

}

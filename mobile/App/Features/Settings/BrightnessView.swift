import SwiftUI
import GaugeCore

struct BrightnessView: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        @Bindable var model = model
        Group {
            if !model.isDemoMode && !model.isNativeSession {
                ContentUnavailableView("Collega il dispositivo", systemImage: "link",
                                       description: Text("Collega Auto Gauge dalla scheda Dispositivo per regolare la luminosità del display."))
            } else {
                Form {
                    Section {
                        Picker("Luminosità", selection: $model.configuration.brightness.mode) {
                            ForEach(BrightnessMode.allCases, id: \.self) { mode in
                                Text(mode.nameIT).tag(mode)
                            }
                        }
                        .accessibilityIdentifier("settings.brightness.mode")
                    } footer: {
                        Text(model.configuration.brightness.mode == .automatic
                             ? "Il display si adatta alla luce dell’ambiente."
                             : "Scegli la luminosità che preferisci per il display.")
                    }
                    if model.configuration.brightness.mode == .manual {
                        Section("Livello di luminosità") {
                            LabeledContent("Luminosità", value: "\(model.configuration.brightness.manualPercent)%")
                                .monospacedDigit()
                            Slider(value: $model.manualBrightness, in: 0...100, step: 1)
                                .frame(minHeight: AppTheme.minimumTouch)
                                .accessibilityLabel("Luminosità del display")
                                .accessibilityValue("\(model.configuration.brightness.manualPercent) per cento")
                        }
                    }
                    DraftActionsView()
                }
                .disabled(model.isSaving)
            }
        }
        .navigationTitle("Luminosità")
    }
}

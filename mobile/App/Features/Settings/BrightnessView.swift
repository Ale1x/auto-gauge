import SwiftUI
import GaugeCore

struct BrightnessView: View {
    @Environment(AppModel.self) private var model
    var body: some View {
        @Bindable var model = model
        Form {
            Section("Modalità") {
                Picker("Controllo luminosità", selection: $model.configuration.brightness.mode) {
                    ForEach(BrightnessMode.allCases, id: \.self) { mode in Text(mode.nameIT).tag(mode) }
                }.pickerStyle(.automatic)
            }
            Section {
                LabeledContent("Livello manuale", value: "\(model.configuration.brightness.manualPercent)%").monospacedDigit()
                Slider(value: $model.manualBrightness, in: 0...100, step: 1)
                    .frame(minHeight: AppTheme.minimumTouch)
                    .accessibilityLabel("Luminosità manuale").accessibilityValue("\(model.configuration.brightness.manualPercent) per cento")
            } header: { Text("Valore manuale") } footer: {
                Text("In automatica il gauge userà il sensore ambiente. Il valore manuale viene conservato; curva e limiti reali del display sono da validare.")
            }
            DraftActionsView()
        }.navigationTitle("Luminosità")
    }
}

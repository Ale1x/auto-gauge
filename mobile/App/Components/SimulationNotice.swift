import SwiftUI

struct SimulationNotice: View {
    @Environment(AppModel.self) private var model
    var body: some View {
        Label(model.isNativeSession ? "Gauge selezionato · archivio reale separato" : "Simulazione · nessun dispositivo reale", systemImage: model.isNativeSession ? "circle.hexagongrid" : "testtube.2")
            .font(.footnote).fixedSize(horizontal: false, vertical: true)
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(.horizontal, AppTheme.spacing).padding(.vertical, AppTheme.smallSpacing)
            .background(AppTheme.panel)
            .accessibilityElement(children: .combine)
    }
}

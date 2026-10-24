import SwiftUI

struct SimulationNotice: View {
    var body: some View {
        Label("Simulazione · nessun dispositivo reale", systemImage: "testtube.2")
            .font(.footnote).fixedSize(horizontal: false, vertical: true)
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(.horizontal, AppTheme.spacing).padding(.vertical, AppTheme.smallSpacing)
            .background(AppTheme.panel)
            .accessibilityElement(children: .combine)
    }
}

import SwiftUI

struct SimulationNotice: View {
    @Environment(AppModel.self) private var model
    var body: some View {
        HStack(spacing: AppTheme.smallSpacing) {
            Label("Modalità demo", systemImage: "sparkles")
                .font(.subheadline.weight(.medium))
            Spacer()
            Button("Esci") { Task { await model.setDemoMode(false) } }
                .font(.subheadline.weight(.semibold))
                .frame(minHeight: AppTheme.minimumTouch)
                .accessibilityLabel("Esci dalla modalità demo")
        }
        .padding(.horizontal, AppTheme.spacing)
        .foregroundStyle(AppTheme.accent)
        .background(AppTheme.panel)
        .accessibilityIdentifier("demo.banner")
    }
}

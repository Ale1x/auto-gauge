import SwiftUI
import GaugeCore

struct OnboardingView: View {
    @Environment(AppModel.self) private var model
    @Environment(\.dynamicTypeSize) private var typeSize
    let onStart: () -> Void
    @State private var step = 0
    @State private var style = 0
    @State private var finishing = false
    private var previewTheme: GaugeTheme {
        GaugeThemeCatalog.find(["original", "pixel_turbo", "synth_sunset"][style]) ?? GaugeThemeCatalog.original
    }
    private var title: String {
        switch step { case 0: "Il tuo display, a modo tuo."; case 1: "Trova il tuo stile."; default: "Pronto a partire?" }
    }
    private var detail: String {
        switch step {
        case 0: "Scegli le informazioni della tua auto e rendile subito riconoscibili."
        case 1: "Prova un’anteprima. Nell’app troverai temi pronti e potrai creare i tuoi colori."
        default: "Accendi Auto Gauge e tienilo vicino al telefono. Oppure esplora subito l’app senza un dispositivo."
        }
    }
    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: AppTheme.pageSpacing) {
                    ProgressView(value: Double(step + 1), total: 3)
                        .accessibilityLabel("Introduzione").accessibilityValue("Passaggio \(step + 1) di 3")
                    Text(title).font(.largeTitle.bold())
                    Text(detail).font(.body).foregroundStyle(.secondary)
                    if step < 2 {
                        GaugePreview(dashboard: model.configuration.dashboards[0], theme: previewTheme)
                            .frame(maxWidth: 240).frame(maxWidth: .infinity).accessibilityHidden(true)
                        if step == 0 {
                            VStack(alignment: .leading, spacing: AppTheme.spacing) {
                                Label("Le misure che ti interessano", systemImage: "gauge.with.needle")
                                Label("Schermate e colori personalizzabili", systemImage: "paintpalette")
                            }.font(.subheadline)
                        } else {
                            if typeSize.isAccessibilitySize { stylePicker.pickerStyle(.menu) }
                            else { stylePicker.pickerStyle(.segmented) }
                            Text("Questa prova non modifica il tuo tema.").font(.subheadline).foregroundStyle(.secondary)
                        }
                    } else {
                        VStack(alignment: .leading, spacing: AppTheme.pageSpacing) {
                            Label("Accendi il dispositivo", systemImage: "power")
                            Label("Avvicina il telefono", systemImage: "iphone")
                            Label("Avvia la ricerca dall’app", systemImage: "magnifyingglass")
                        }.font(.headline).modifier(AppCardStyle())
                    }
                }
                .padding(AppTheme.pageSpacing)
                .frame(maxWidth: AppTheme.readableWidth).frame(maxWidth: .infinity)
            }
            .background(AppTheme.canvas.ignoresSafeArea())
            .safeAreaInset(edge: .bottom) {
                VStack(spacing: AppTheme.smallSpacing) {
                    if step < 2 {
                        Button { step += 1 } label: { Text("Continua").frame(maxWidth: .infinity, minHeight: AppTheme.minimumTouch) }
                            .buttonStyle(.borderedProminent).foregroundStyle(AppTheme.canvas)
                            .accessibilityIdentifier("onboarding.next")
                    } else {
                        Button { finish(demo: false) } label: { Text("Collega Auto Gauge").frame(maxWidth: .infinity, minHeight: AppTheme.minimumTouch) }
                            .buttonStyle(.borderedProminent).foregroundStyle(AppTheme.canvas)
                            .accessibilityIdentifier("onboarding.connect")
                        Button { finish(demo: true) } label: { Text("Prova senza dispositivo").frame(maxWidth: .infinity, minHeight: AppTheme.minimumTouch) }
                            .buttonStyle(.bordered).accessibilityIdentifier("onboarding.demo")
                    }
                    if finishing { ProgressView("Un momento…") }
                }
                .controlSize(.large)
                .padding(AppTheme.pageSpacing).frame(maxWidth: .infinity)
                .background(AppTheme.canvas)
                .disabled(finishing)
            }
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    if step > 0 { Button("Indietro", systemImage: "chevron.left") { step -= 1 }.disabled(finishing) }
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Salta") { model.completeOnboarding() }.disabled(finishing).accessibilityIdentifier("onboarding.skip")
                }
            }
        }
        .tint(AppTheme.accent).interactiveDismissDisabled()
    }
    private var stylePicker: some View {
        Picker("Stile dell’anteprima", selection: $style) {
            Text("Essenziale").tag(0); Text("Pixel").tag(1); Text("Neon").tag(2)
        }.accessibilityIdentifier("onboarding.styles")
    }
    private func finish(demo: Bool) {
        guard !finishing else { return }; finishing = true
        Task {
            await model.setDemoMode(demo)
            onStart()
            model.completeOnboarding()
        }
    }
}

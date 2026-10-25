import SwiftUI

struct OnboardingView: View {
    @Environment(AppModel.self) private var model
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    let onStart: () -> Void
    @State private var page = OnboardingPage.welcome
    @State private var finishing = false

    var body: some View {
        NavigationStack {
            GeometryReader { geometry in
                TabView(selection: $page) {
                    ForEach(OnboardingPage.allCases) { page in
                        OnboardingPageView(page: page, availableHeight: geometry.size.height)
                            .tag(page)
                    }
                }
                .tabViewStyle(.page(indexDisplayMode: .never))
                .disabled(finishing)
            }
            .background(AppTheme.canvas.ignoresSafeArea())
            .safeAreaInset(edge: .bottom, spacing: 0) {
                VStack(spacing: AppTheme.spacing) {
                    HStack(spacing: AppTheme.smallSpacing) {
                        ForEach(OnboardingPage.allCases) { item in
                            Capsule()
                                .fill(item == page ? AppTheme.accent : Color.secondary.opacity(0.25))
                                .frame(width: item == page ? 24 : 8, height: 8)
                        }
                    }
                    .accessibilityElement(children: .ignore)
                    .accessibilityLabel("Introduzione")
                    .accessibilityValue("Passaggio \(page.rawValue + 1) di 3")
                    .accessibilityAdjustableAction { direction in
                        switch direction {
                        case .increment: move(by: 1)
                        case .decrement: move(by: -1)
                        @unknown default: break
                        }
                    }
                    if page != .connect {
                        Button { move(by: 1) } label: {
                            Text("Continua")
                                .multilineTextAlignment(.center)
                                .fixedSize(horizontal: false, vertical: true)
                                .frame(maxWidth: .infinity, minHeight: AppTheme.minimumTouch)
                        }
                        .buttonStyle(.borderedProminent).foregroundStyle(AppTheme.canvas)
                        .accessibilityIdentifier("onboarding.next")
                    } else {
                        Button { finish(demo: false) } label: {
                            Text("Collega Auto Gauge")
                                .multilineTextAlignment(.center)
                                .fixedSize(horizontal: false, vertical: true)
                                .frame(maxWidth: .infinity, minHeight: AppTheme.minimumTouch)
                        }
                        .buttonStyle(.borderedProminent).foregroundStyle(AppTheme.canvas)
                        .accessibilityIdentifier("onboarding.connect")
                        Button { finish(demo: true) } label: {
                            Text("Prova senza dispositivo")
                                .multilineTextAlignment(.center)
                                .fixedSize(horizontal: false, vertical: true)
                                .frame(maxWidth: .infinity, minHeight: AppTheme.minimumTouch)
                        }
                        .buttonStyle(.plain).foregroundStyle(AppTheme.accent)
                        .accessibilityIdentifier("onboarding.demo")
                    }
                    if finishing { ProgressView("Un momento…") }
                }
                .controlSize(.large)
                .padding(.horizontal, AppTheme.pageSpacing)
                .padding(.top, AppTheme.spacing)
                .padding(.bottom, AppTheme.spacing)
                .frame(maxWidth: 480).frame(maxWidth: .infinity)
                .background(AppTheme.canvas)
                .disabled(finishing)
            }
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    if page != .welcome {
                        Button("Indietro", systemImage: "chevron.left") { move(by: -1) }
                            .disabled(finishing)
                            .accessibilityIdentifier("onboarding.back")
                    }
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Salta", action: model.completeOnboarding)
                        .disabled(finishing).accessibilityIdentifier("onboarding.skip")
                }
            }
        }
        .tint(AppTheme.accent).interactiveDismissDisabled()
    }

    private func move(by offset: Int) {
        guard !finishing, let destination = OnboardingPage(rawValue: page.rawValue + offset) else { return }
        withAnimation(reduceMotion ? nil : .easeInOut(duration: 0.25)) {
            page = destination
        }
    }

    private func finish(demo: Bool) {
        guard !finishing else { return }
        finishing = true
        Task {
            await model.setDemoMode(demo)
            onStart()
            model.completeOnboarding()
        }
    }
}

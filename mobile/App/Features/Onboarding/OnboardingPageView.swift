import SwiftUI

struct OnboardingPageView: View {
    @Environment(\.dynamicTypeSize) private var typeSize
    let page: OnboardingPage
    let availableHeight: CGFloat

    var body: some View {
        ScrollView {
            VStack(spacing: AppTheme.pageSpacing) {
                Image(decorative: page.artwork)
                    .resizable()
                    .scaledToFit()
                    .frame(maxWidth: 360)
                    .frame(height: typeSize.isAccessibilitySize ? 150 : min(360, max(160, availableHeight * 0.56)))
                    .accessibilityHidden(true)
                VStack(spacing: AppTheme.spacing) {
                    Text(page.title)
                        .font(.largeTitle.bold())
                        .foregroundStyle(.primary)
                        .accessibilityAddTraits(.isHeader)
                        .accessibilityIdentifier("onboarding.heading.\(page.rawValue)")
                    Text(page.detail)
                        .font(.body)
                        .foregroundStyle(.secondary)
                        .frame(maxWidth: 340)
                }
                .multilineTextAlignment(.center)
                .fixedSize(horizontal: false, vertical: true)
            }
            .padding(AppTheme.pageSpacing)
            .frame(maxWidth: AppTheme.readableWidth)
            .frame(maxWidth: .infinity, minHeight: availableHeight)
        }
        .scrollIndicators(.hidden)
    }
}

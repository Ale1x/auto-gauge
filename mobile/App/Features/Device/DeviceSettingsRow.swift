import SwiftUI

struct DeviceSettingsRow: View {
    let title: LocalizedStringResource
    let detail: LocalizedStringResource
    let symbol: String
    var body: some View {
        HStack(spacing: AppTheme.spacing) {
            Label {
                VStack(alignment: .leading, spacing: 4) {
                    Text(title).foregroundStyle(Color.primary)
                    Text(detail).font(.subheadline).foregroundStyle(Color.secondary)
                }
            } icon: { Image(systemName: symbol).foregroundStyle(AppTheme.accent).accessibilityHidden(true) }
            Spacer(minLength: 0)
            Image(systemName: "chevron.right").font(.subheadline).foregroundStyle(.tertiary).accessibilityHidden(true)
        }
        .frame(maxWidth: .infinity, minHeight: AppTheme.minimumTouch, alignment: .leading)
        .padding(.vertical, AppTheme.smallSpacing)
    }
}

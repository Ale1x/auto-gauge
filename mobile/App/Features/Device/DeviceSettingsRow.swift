import SwiftUI

struct DeviceSettingsRow: View {
    let title: LocalizedStringResource
    let detail: LocalizedStringResource
    let symbol: String
    var body: some View {
        Label {
            VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
                Text(title).foregroundStyle(.primary)
                Text(detail).font(.subheadline).foregroundStyle(.secondary)
            }
        } icon: { Image(systemName: symbol).foregroundStyle(AppTheme.accent).accessibilityHidden(true) }
        .frame(maxWidth: .infinity, minHeight: AppTheme.minimumTouch, alignment: .leading)
        .padding(.vertical, AppTheme.smallSpacing)
    }
}

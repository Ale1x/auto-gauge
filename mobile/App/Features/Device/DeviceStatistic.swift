import SwiftUI

struct DeviceStatistic: View {
    let title: LocalizedStringResource
    let value: String
    var body: some View {
        VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
            Text(title).font(.subheadline).foregroundStyle(.secondary)
            Text(value).font(.title3).bold().monospacedDigit()
        }.accessibilityElement(children: .combine)
    }
}

import SwiftUI

struct AppActionButton: View {
    let title: LocalizedStringResource
    let symbol: String
    var disabled = false
    var role: ButtonRole? = nil
    let action: () -> Void
    var body: some View {
        Button(role: role, action: action) {
            Label(title, systemImage: symbol)
                .frame(maxWidth: .infinity, minHeight: AppTheme.minimumTouch, alignment: .leading)
                .contentShape(Rectangle())
                .multilineTextAlignment(.leading)
        }.buttonStyle(.borderless).disabled(disabled)
    }
}

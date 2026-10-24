import SwiftUI

struct AppCardStyle: ViewModifier {
    func body(content: Content) -> some View {
        content.padding(AppTheme.spacing)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(AppTheme.panel, in: .rect(cornerRadius: AppTheme.cardRadius))
    }
}

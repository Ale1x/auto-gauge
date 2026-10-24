import SwiftUI

/// Reserve banner space inside navigation content, leaving the system title/back bar visible.
struct SimulationBannerModifier: ViewModifier {
    func body(content: Content) -> some View {
        content.safeAreaInset(edge: .top, spacing: 0) { SimulationNotice() }
    }
}

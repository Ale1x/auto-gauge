import SwiftUI

@main
struct AutoGaugeApp: App {
    @State private var model = AppModel()
    var body: some Scene {
        WindowGroup { RootView().environment(model) }
    }
}

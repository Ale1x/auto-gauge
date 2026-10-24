import SwiftUI

struct DeviceView: View {
    var body: some View {
        ScrollView { DeviceContentView() }
            .background(AppTheme.canvas)
            .navigationTitle("Auto Gauge").navigationBarTitleDisplayMode(.inline)
    }
}

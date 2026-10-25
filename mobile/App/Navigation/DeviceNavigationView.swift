import SwiftUI

struct DeviceNavigationView: View {
    @Binding var path: [DeviceRoute]
    var body: some View {
        NavigationStack(path: $path) {
            DeviceView().navigationDestination(for: DeviceRoute.self) { route in
                Group {
                switch route {
                case .screens: DashboardListView(showsHeading: false)
                case .brightness: BrightnessView()
                case .catalog: CatalogView()
                case .firmware: FirmwareView()
                case .bluetoothDevelopment: BluetoothDevelopmentView()
                }
                }.toolbarVisibility(.visible, for: .navigationBar)
            }
            .navigationDestination(for: DashboardRoute.self) { route in
                switch route { case .editor(let id): DashboardEditorView(dashboardID: id).toolbarVisibility(.visible, for: .navigationBar) }
            }
        }
    }
}

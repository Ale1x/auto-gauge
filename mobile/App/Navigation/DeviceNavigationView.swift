import SwiftUI

struct DeviceNavigationView: View {
    @Binding var path: [DeviceRoute]
    var body: some View {
        NavigationStack(path: $path) {
            DeviceView().modifier(SimulationBannerModifier()).navigationDestination(for: DeviceRoute.self) { route in
                Group {
                switch route {
                case .brightness: BrightnessView()
                case .catalog: CatalogView()
                case .firmware: FirmwareView()
                case .bluetoothDevelopment: BluetoothDevelopmentView()
                }
                }.modifier(SimulationBannerModifier())
            }
        }
    }
}

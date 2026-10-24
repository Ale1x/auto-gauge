import SwiftUI

struct DeviceNavigationView: View {
    @Binding var path: [DeviceRoute]
    var body: some View {
        NavigationStack(path: $path) {
            DeviceView().navigationDestination(for: DeviceRoute.self) { route in
                switch route {
                case .brightness: BrightnessView()
                case .catalog: CatalogView()
                case .firmware: FirmwareView()
                case .bluetoothDevelopment: BluetoothDevelopmentView()
                }
            }
        }
    }
}

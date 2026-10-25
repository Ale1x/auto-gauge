import SwiftUI

struct FirmwareView: View {
    @Environment(AppModel.self) private var model
    var body: some View {
        FirmwarePreparationView(trust: model.firmwareTrust, installation: model.firmwareInstallation)
    }
}

import SwiftUI

struct FirmwareView: View {
    @Environment(AppModel.self) private var model
    var body: some View {
        Group {
            if model.isDemoMode { DemoFirmwareView() }
            else if model.isNativeSession && model.firmwareInstallation.configured && model.firmwareTrust != nil {
                FirmwarePreparationView(trust: model.firmwareTrust, installation: model.firmwareInstallation)
            } else {
                ContentUnavailableView("Aggiornamenti non disponibili", systemImage: "arrow.down.circle", description: Text(model.isNativeSession ? "L’aggiornamento di questo dispositivo non è ancora disponibile." : "Collega Auto Gauge per controllare gli aggiornamenti."))
            }
        }
        .navigationTitle("Aggiornamenti").navigationBarTitleDisplayMode(.inline)
    }
}

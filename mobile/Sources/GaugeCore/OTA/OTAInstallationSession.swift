import Foundation

/// A locally authorized, exclusive target OTA route. The platform provider owns
/// route selection and must recheck authorization/lease for every operation.
/// Returning a session is a trusted local decision, never a wire/hello assertion.
public protocol OTAInstallationSession: OTATransferTransport {
    func close() async
}

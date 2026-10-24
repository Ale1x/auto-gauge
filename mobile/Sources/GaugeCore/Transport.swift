import Foundation

public enum GaugeTransportError: Error, LocalizedError {
    case authenticationNotConfigured, unavailable
    public var errorDescription: String? {
        switch self {
        case .authenticationNotConfigured: return "Associazione e sessione autenticata non ancora configurate."
        case .unavailable: return "Dispositivo non collegato."
        }
    }
}

/// Implementations must validate authentication/capabilities before mutating a device.
public protocol GaugeTransport: Sendable {
    func supportedPIDs() async throws -> [PIDDescriptor]
    func configuration() async throws -> GaugeConfiguration
    func apply(_ configuration: GaugeConfiguration) async throws
}

/// Real transport deliberately refuses access until the security protocol is implemented.
public struct UnconfiguredGaugeTransport: GaugeTransport {
    public init() {}
    public func supportedPIDs() async throws -> [PIDDescriptor] { throw GaugeTransportError.unavailable }
    public func configuration() async throws -> GaugeConfiguration { throw GaugeTransportError.unavailable }
    public func apply(_ configuration: GaugeConfiguration) async throws { throw GaugeTransportError.authenticationNotConfigured }
}

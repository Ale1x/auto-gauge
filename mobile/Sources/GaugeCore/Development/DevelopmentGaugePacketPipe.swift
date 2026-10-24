/// Explicit local development connection. NativeBluetoothController intentionally does NOT
/// conform: this transport must not enable radio configuration/logs/mutations.
public protocol DevelopmentGaugePacketPipe: GaugePacketPipe {
    func connect() async throws
}

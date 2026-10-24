import CoreBluetooth

@MainActor
final class BluetoothPeripheralDelegate: NSObject, @preconcurrency CBPeripheralDelegate {
    weak var controller: NativeBluetoothController?
    let generation: UInt64
    init(controller: NativeBluetoothController, generation: UInt64) { self.controller = controller; self.generation = generation }
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: (any Error)?) {
        controller?.services(peripheral, error: error, generation: generation)
    }
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: (any Error)?) {
        controller?.characteristics(peripheral, service: service, error: error, generation: generation)
    }
    func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: (any Error)?) {
        controller?.subscription(peripheral, characteristic: characteristic, error: error, generation: generation)
    }
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: (any Error)?) {
        controller?.incoming(peripheral, characteristic: characteristic, error: error, generation: generation)
    }
    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: (any Error)?) {
        controller?.writeCompleted(peripheral, characteristic: characteristic, error: error, generation: generation)
    }
}

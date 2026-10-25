import Foundation
import Observation
import CoreBluetooth
import GaugeCore

/// CBCentralManager is nil until the person explicitly presses the development scan button.
@MainActor @Observable
final class NativeBluetoothController: NSObject, @preconcurrency CBCentralManagerDelegate, GaugePacketPipe {
    var developmentEnabled = false
    private(set) var candidates: [BluetoothCandidate] = []
    private(set) var isScanning = false
    private(set) var isConnecting = false
    private(set) var isReady = false
    private(set) var rpcBusy = false
    private(set) var notice = "Bluetooth reale disattivato. Il simulatore resta separato."
    private let profile = try? GaugeGATTProfile.development()
    private var central: CBCentralManager?
    private var found: [UUID: CBPeripheral] = [:]
    private var active: CBPeripheral?
    private var proxy: BluetoothPeripheralDelegate?
    private var service: CBService?
    private var requestCharacteristic: CBCharacteristic?
    private var responseCharacteristic: CBCharacteristic?
    private var generation: UInt64 = 0
    private var uiEpoch: UInt64 = 0
    private var scanRequested = false
    private var timer: Task<Void, Never>?
    private var writeWaiter: CheckedContinuation<Void, any Error>?
    private var readWaiter: CheckedContinuation<GaugeIncomingChunk, any Error>?
    private var incomingChunks: [GaugeIncomingChunk] = []
    private var incomingBytes = 0
    private var incomingDecoder = GaugeStreamReassembler()
    private var frameTimer: Task<Void, Never>?
    private var rpc: GaugeRPCClient?
    private var qualifiedClaim = false
    var onSessionInvalidated: (@MainActor (UInt64) -> Void)?
    var connectionStatusText: String {
        switch central?.state {
        case .unauthorized: return "Consenti l’accesso al Bluetooth nelle impostazioni del telefono."
        case .poweredOff: return "Attiva il Bluetooth sul telefono per continuare."
        case .unsupported: return "Questo telefono non supporta il collegamento."
        case .resetting: return "Il Bluetooth si sta riavviando. Attendi un momento."
        default: return isReady ? "Dispositivo trovato. Puoi collegarlo qui sotto." : "Tocca Cerca dispositivo per iniziare."
        }
    }
    func isCurrentConnection(_ lease: UInt64) -> Bool { isReady && generation == lease }
    func qualifiedPacketPipe() async throws -> any GaugePacketPipe {
        guard rpc == nil, !rpcBusy, !qualifiedClaim else { throw GaugeLinkError.busy }
        let ready = try await readyConnection()
        return NativeQualifiedPacketPipe(controller: self, generation: ready.generation)
    }
    func beginQualifiedRequest(generation lease: UInt64) throws {
        guard isReady, lease == generation, rpc == nil, !rpcBusy,
              incomingChunks.isEmpty, incomingDecoder.bufferedBytes == 0 else { throw GaugeLinkError.notReady }
        qualifiedClaim = true; rpcBusy = true
    }
    func endQualifiedRequest(generation lease: UInt64) throws {
        guard isReady, lease == generation, qualifiedClaim, rpcBusy,
              incomingChunks.isEmpty, incomingDecoder.bufferedBytes == 0, readWaiter == nil, writeWaiter == nil else {
            throw GaugeLinkError.invalidReply
        }
        rpcBusy = false
    }
    func beginScan() {
        guard developmentEnabled, profile != nil else { notice = "Abilita prima la modalità sviluppo Bluetooth."; return }
        uiEpoch &+= 1
        close(message: "Avvio ricerca di sviluppo · massimo 10 secondi")
        scanRequested = true; isScanning = true
        let epoch = uiEpoch
        // The only constructor call: no app launch, fixture, background task or cloud path reaches it.
        central = CBCentralManager(delegate: self, queue: .main)
        timer = Task { [weak self] in
            do { try await Task.sleep(for: .seconds(10)) } catch { return }
            guard let self, self.uiEpoch == epoch, self.isScanning else { return }
            self.central?.stopScan(); self.scanRequested = false; self.isScanning = false
            self.notice = "Ricerca terminata. Nessuna connessione avviata automaticamente."
        }
    }
    func connect(_ id: UUID) {
        guard developmentEnabled, !isConnecting, !isReady, let central, central.state == .poweredOn,
              let peripheral = found[id] else { return }
        uiEpoch &+= 1; generation &+= 1
        timer?.cancel(); central.stopScan(); scanRequested = false; isScanning = false
        active = peripheral; isConnecting = true
        let lease = generation
        proxy = BluetoothPeripheralDelegate(controller: self, generation: lease); peripheral.delegate = proxy
        notice = "Connessione e attivazione indicazioni in corso. Sicurezza server non verificata."
        central.connect(peripheral)
        timer = Task { [weak self] in
            do { try await Task.sleep(for: .seconds(30)) } catch { return }
            guard let self, self.generation == lease, !self.isReady else { return }
            self.close(message: "Connessione non pronta entro il limite. Ripeti la ricerca solo se necessario.")
        }
    }
    func stop() { uiEpoch &+= 1; close(message: "Bluetooth di sviluppo arrestato.") }
    func probeHello() async {
        guard isReady, !rpcBusy, !qualifiedClaim else { return }
        rpcBusy = true; let epoch = uiEpoch
        do {
            let client: GaugeRPCClient
            if let rpc { client = rpc } else {
                client = GaugeRPCClient(pipe: self); rpc = client
                try await client.open()
            }
            let reply = try await client.request(.hello, body: Data("{\"op\":\"hello\"}".utf8))
            guard epoch == uiEpoch else { return }
            notice = reply.simulation == true ? "Risposta hello di un endpoint dichiarato simulato. Nessuna autenticazione reale dimostrata." : "Risposta hello valida. Non dimostra LESC, MITM, chiave o ammissione; nessuna mutazione consentita."
        } catch {
            guard epoch == uiEpoch else { return }
            notice = error.localizedDescription
        }
        guard epoch == uiEpoch else { return }
        if !incomingChunks.isEmpty || incomingDecoder.bufferedBytes != 0 {
            close(message: "Dati inattesi dopo la risposta. Collegamento chiuso senza riuso dei frammenti.")
        }
        rpcBusy = false
    }
    func readyConnection() async throws -> GaugePipeConnection {
        guard isReady, let active, responseCharacteristic?.isNotifying == true else { throw GaugeLinkError.notReady }
        let maximum = active.maximumWriteValueLength(for: .withResponse)
        guard maximum > 0 else { throw GaugeLinkError.notReady }
        return .init(generation: generation, maximumWriteBytes: min(maximum, 512))
    }
    func write(_ data: Data, generation lease: UInt64) async throws {
        guard isReady, lease == generation, let active, let requestCharacteristic,
              !data.isEmpty, data.count <= min(active.maximumWriteValueLength(for: .withResponse), 512), writeWaiter == nil else { throw GaugeLinkError.notReady }
        try await withTaskCancellationHandler {
            try Task.checkCancellation()
            try await withCheckedThrowingContinuation { continuation in
                writeWaiter = continuation
                active.writeValue(data, for: requestCharacteristic, type: .withResponse)
            }
        } onCancel: { Task { @MainActor [weak self] in if self?.generation == lease { self?.close(message: "Scrittura annullata: collegamento chiuso.") } } }
    }
    func receive(generation lease: UInt64) async throws -> GaugeIncomingChunk {
        guard isReady, lease == generation, readWaiter == nil else { throw GaugeLinkError.disconnected }
        if !incomingChunks.isEmpty {
            let first = incomingChunks.removeFirst(); incomingBytes -= first.data.count; return first
        }
        return try await withTaskCancellationHandler {
            try Task.checkCancellation()
            return try await withCheckedThrowingContinuation { readWaiter = $0 }
        } onCancel: { Task { @MainActor [weak self] in if self?.generation == lease { self?.close(message: "Lettura annullata: collegamento chiuso.") } } }
    }
    func disconnect(generation lease: UInt64) async {
        guard lease == generation else { return }
        close(message: "Collegamento chiuso. Nessuna autenticazione o applicazione reale confermata.")
    }
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        guard central === self.central else { return }
        if central.state == .poweredOn {
            if scanRequested, let profile { central.scanForPeripherals(withServices: [CBUUID(nsuuid: profile.serviceUUID)], options: [CBCentralManagerScanOptionAllowDuplicatesKey: false]) }
        } else if central.state != .unknown {
            close(message: central.state == .unauthorized ? "Accesso Bluetooth non autorizzato nelle Impostazioni." : "Bluetooth non disponibile. Nessun tentativo automatico.")
        }
    }
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        guard central === self.central, isScanning, found[peripheral.identifier] == nil, found.count < 20 else { return }
        found[peripheral.identifier] = peripheral
        candidates.append(.init(id: peripheral.identifier, name: String((peripheral.name ?? "Gauge senza nome").prefix(64))))
    }
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        guard central === self.central, active === peripheral, isConnecting, let profile else { return }
        peripheral.discoverServices([CBUUID(nsuuid: profile.serviceUUID)])
    }
    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: (any Error)?) {
        guard central === self.central, active === peripheral else { return }; close(message: "Connessione non riuscita. Nessun nuovo tentativo automatico.")
    }
    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: (any Error)?) {
        guard central === self.central, active === peripheral else { return }; close(message: "Dispositivo disconnesso. Richieste e frammenti invalidati.")
    }
    private func close(message: String) {
        let invalidatedQualifiedSession = qualifiedClaim
        let invalidatedGeneration = generation
        qualifiedClaim = false
        generation &+= 1; timer?.cancel(); timer = nil
        frameTimer?.cancel(); frameTimer = nil; incomingDecoder.reset()
        central?.stopScan(); central?.delegate = nil
        if let active, active.state != .disconnected { central?.cancelPeripheralConnection(active) }
        active?.delegate = nil; active = nil; proxy = nil; central = nil
        service = nil; requestCharacteristic = nil; responseCharacteristic = nil
        found.removeAll(); candidates.removeAll(); incomingChunks.removeAll(); incomingBytes = 0
        scanRequested = false; isScanning = false; isConnecting = false; isReady = false; rpcBusy = false; rpc = nil
        let write = writeWaiter; writeWaiter = nil; write?.resume(throwing: GaugeLinkError.disconnected)
        let read = readWaiter; readWaiter = nil; read?.resume(throwing: GaugeLinkError.disconnected)
        notice = message
        if invalidatedQualifiedSession { onSessionInvalidated?(invalidatedGeneration) }
    }
    private func valid(_ peripheral: CBPeripheral, lease: UInt64) -> Bool { lease == generation && active === peripheral }
    func services(_ peripheral: CBPeripheral, error: (any Error)?, generation lease: UInt64) {
        guard valid(peripheral, lease: lease), isConnecting else { return }
        guard error == nil, let profile, let service = peripheral.services?.first(where: { $0.uuid == CBUUID(nsuuid: profile.serviceUUID) }) else { close(message: "Servizio di sviluppo non disponibile."); return }
        self.service = service
        peripheral.discoverCharacteristics([CBUUID(nsuuid: profile.requestUUID), CBUUID(nsuuid: profile.responseUUID)], for: service)
    }
    func characteristics(_ peripheral: CBPeripheral, service: CBService, error: (any Error)?, generation lease: UInt64) {
        guard valid(peripheral, lease: lease), isConnecting, service === self.service else { return }
        guard error == nil, let profile,
              let request = service.characteristics?.first(where: { $0.uuid == CBUUID(nsuuid: profile.requestUUID) && $0.properties.contains(.write) }),
              let response = service.characteristics?.first(where: { $0.uuid == CBUUID(nsuuid: profile.responseUUID) && $0.properties.contains(.indicate) && !$0.properties.contains(.notify) }) else { close(message: "Caratteristiche Write With Response/indicazioni non disponibili."); return }
        requestCharacteristic = request; responseCharacteristic = response
        peripheral.setNotifyValue(true, for: response)
    }
    func subscription(_ peripheral: CBPeripheral, characteristic: CBCharacteristic, error: (any Error)?, generation lease: UInt64) {
        guard valid(peripheral, lease: lease), characteristic === responseCharacteristic else { return }
        guard error == nil, characteristic.isNotifying, peripheral.maximumWriteValueLength(for: .withResponse) > 0 else { close(message: "Indicazioni non attivate. Nessuna richiesta inviata."); return }
        timer?.cancel(); timer = nil; isConnecting = false; isReady = true
        notice = "Canale di sviluppo pronto. Autenticazione server non dimostrata; configurazione, log e mutazioni reali restano esclusi dalla UI."
    }
    func incoming(_ peripheral: CBPeripheral, characteristic: CBCharacteristic, error: (any Error)?, generation lease: UInt64) {
        guard valid(peripheral, lease: lease), characteristic === responseCharacteristic, isReady else { return }
        guard error == nil, let data = characteristic.value, !data.isEmpty, data.count <= 512 else { close(message: "Indicazione non valida. Collegamento chiuso."); return }
        guard rpcBusy else { close(message: "Indicazione non richiesta: nessuna lettura privata automatica."); return }
        do {
            let now = ContinuousClock().now
            let frames = try incomingDecoder.feed(data, at: now)
            frameTimer?.cancel(); frameTimer = nil
            if let deadline = incomingDecoder.deadline {
                frameTimer = Task { [weak self] in
                    do { try await ContinuousClock().sleep(until: deadline) } catch { return }
                    guard let self, self.generation == lease, self.incomingDecoder.deadline == deadline else { return }
                    self.close(message: "Frame incompleto oltre 10 secondi. Collegamento chiuso.")
                }
            }
            // Validate at notification arrival, including while the last ATT ACK is pending.
            // Only complete validated frames enter the separate bounded delivery queue.
            for frame in frames {
                let packet = try GaugeFrameCodec.encode(frame)
                for offset in stride(from: 0, to: packet.count, by: 512) {
                    let chunk = GaugeIncomingChunk(data: packet.subdata(in: offset..<min(packet.count, offset + 512)), receivedAt: now)
                    if let readWaiter { self.readWaiter = nil; readWaiter.resume(returning: chunk) }
                    else {
                        guard incomingBytes + chunk.data.count <= 4112 else { close(message: "Coda Bluetooth piena. Nessun dato troncato o riutilizzato."); return }
                        incomingChunks.append(chunk); incomingBytes += chunk.data.count
                    }
                }
            }
        } catch { close(message: error.localizedDescription) }
    }

    func writeCompleted(_ peripheral: CBPeripheral, characteristic: CBCharacteristic, error: (any Error)?, generation lease: UInt64) {
        guard valid(peripheral, lease: lease), characteristic === requestCharacteristic else { return }
        if error != nil { close(message: "Scrittura ATT non confermata. Nessun successo applicativo dichiarato."); return }
        let waiter = writeWaiter; writeWaiter = nil; waiter?.resume()
    }
}

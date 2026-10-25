import StoreKit
import Observation

/// The release gate remains off until App Store products and device delivery
/// are configured. Demo never starts StoreKit requests or transactions.
@MainActor @Observable
final class ThemeStoreClient {
    let enabled: Bool
    private(set) var products: [Product] = []
    private(set) var owned: Set<String> = []
    private(set) var busy = false
    private(set) var lastProductID: String?
    private(set) var notice: String?
    private(set) var diagnosticDetail: String?
    @ObservationIgnored private var listener: Task<Void, Never>?
    private var generation = 0
    init() { enabled = Bundle.main.object(forInfoDictionaryKey: "AutoGaugeThemePurchasesEnabled") as? Bool ?? false }
    func stop() { generation += 1; listener?.cancel(); listener = nil; busy = false; products = []; owned = []; notice = nil; lastProductID = nil }
    func start(ids: [String]) async {
        stop(); guard enabled else { return }
        let token = generation; busy = true
        defer { if token == generation { busy = false } }
        do {
            let loaded = try await Product.products(for: ids).filter { $0.type == .nonConsumable }
            guard token == generation else { return }; products = loaded
            await refresh(ids: Set(ids), token: token)
            guard token == generation else { return }
            listener = Task { [weak self] in
                for await update in Transaction.updates {
                    guard let self, !Task.isCancelled, token == self.generation else { return }
                    guard case .verified(let transaction) = update, ids.contains(transaction.productID), transaction.productType == .nonConsumable else { continue }
                    await self.refresh(ids: Set(ids), token: token)
                    guard token == self.generation else { return }; await transaction.finish()
                }
            }
        } catch { if token == generation { diagnosticDetail = String(describing: error); notice = "Lo store non è disponibile. Puoi continuare a provare i temi." } }
    }
    func buy(_ product: Product, perform: @MainActor (Product) async throws -> Product.PurchaseResult) async {
        guard enabled, !busy, products.contains(where: { $0.id == product.id }) else { return }
        let token = generation; busy = true; notice = nil
        defer { if token == generation { busy = false } }
        do {
            lastProductID = product.id
            let result = try await perform(product)
            guard token == generation else { return }
            switch result {
            case .success(.verified(let transaction)):
                guard token == generation, transaction.productID == product.id, transaction.productType == .nonConsumable, transaction.revocationDate == nil else { return }
                owned.insert(transaction.productID)
                await transaction.finish(); notice = "Tema aggiunto alla raccolta"
            case .success(.unverified): notice = "Non è stato possibile confermare l’acquisto. Riprova dallo store."
            case .pending: notice = "L’acquisto è in attesa di conferma."
            case .userCancelled: break
            @unknown default: notice = "Acquisto non completato."
            }
        } catch { if token == generation { diagnosticDetail = String(describing: error); notice = "Acquisto non completato. Puoi riprovare." } }
    }
    func restore() async {
        guard enabled, !busy else { notice = "Gli acquisti saranno disponibili con l’apertura dello store."; return }
        let token = generation; busy = true
        defer { if token == generation { busy = false } }
        do {
            try await AppStore.sync()
            guard token == generation else { return }
            await refresh(ids: Set(products.map(\.id)), token: token)
            if token == generation { notice = "Acquisti ripristinati" }
        } catch { if token == generation { diagnosticDetail = String(describing: error); notice = "Non è stato possibile recuperare gli acquisti. Riprova." } }
    }
    private func refresh(ids: Set<String>, token: Int) async {
        var current: Set<String> = []
        for await result in Transaction.currentEntitlements {
            guard token == generation else { return }
            if case .verified(let transaction) = result, ids.contains(transaction.productID), transaction.revocationDate == nil, transaction.productType == .nonConsumable { current.insert(transaction.productID) }
        }
        if token == generation { owned = current }
    }
}

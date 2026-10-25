import SwiftUI

enum OnboardingPage: Int, CaseIterable, Identifiable {
    case welcome, themes, connect

    var id: Self { self }

    var artwork: String {
        switch self {
        case .welcome: "OnboardingWelcome"
        case .themes: "OnboardingThemes"
        case .connect: "OnboardingConnect"
        }
    }

    var title: LocalizedStringKey {
        switch self {
        case .welcome: "La tua auto,\na colpo d’occhio."
        case .themes: "Il tuo stile.\nOgni volta che vuoi."
        case .connect: "Si parte.\nBastano pochi tocchi."
        }
    }

    var detail: LocalizedStringKey {
        switch self {
        case .welcome: "Velocità, temperature e le informazioni che contano, sul tuo display."
        case .themes: "Dai quadranti classici al mondo pixel. Scegli un tema o crea i tuoi colori."
        case .connect: "Accendi Auto Gauge e avvicina il telefono. Ti guidiamo noi al collegamento."
        }
    }
}

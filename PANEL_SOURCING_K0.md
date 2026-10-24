# Pannelli K0 — ricerca di secondo livello, 2026-10-24

Nessun acquisto, contatto o disponibilità riservata. Candidati nudi con touch, senza PCB elaborativo; una pagina di vendita non chiude O-16. I pannelli con codici diversi non condividono automaticamente il pinout.

| Candidato / canale | Evidenza commerciale | Evidenza tecnica | Esito |
|---|---|---|---|
| Huaxin ESHX0175Y003AMT003, pagina HX0175Y003AMT003 | E18: MOQ pubblico 1000, lead time 4–6 settimane; cinque campioni non confermati | E01 conservato; incongruenze già registrate | Campioni e identità codice da chiedere; non è una fornitura lotto5 verificata |
| VIEWE ALL-UEOL018VG-RA31-A001A | E19: prezzo pubblico USD14,99 e vendita diretta; stock/lotto5/spedizione non confermati | E20 PDF V1.0 originale acquisito; copertina WV invece di VG, cartiglio ancora diverso | Candidato interessante per sourcing, non congelabile |
| LX Display LX175A3101CTP01 tramite Özdisan / Eurocoin | E21: stock0, MOQ1, lead time dichiarato4–5 settimane; E22 solo richiesta offerta | E21 indica 51,5×51,5 mm, E22 touch CST820; datasheet esatto non ottenuto | Alternativa da investigare, non sostituto equivalente |

E19 pubblica un pannello, non una devboard. Il suo prezzo implica USD74,95 per cinque soli pannelli prima di trasporto/imposte; non è un preventivo dell'assieme. E18 riporta una fascia prezzo troppo ampia per usarla nel budget. E21 non offre stock disponibile.

## Verifica critica E20 (PDF VIEWE)

Copertina: ALL-UEOL018WV-RA31-A001A; cartiglio p.6: ALL-UE015WV-RB24-A021A. La cover risulta48,16 mm, diversa da E01. P.5 contiene temperature non coerenti con la pagina commerciale. P.6 annota I2C1,8V; non collegare direttamente i segnali a3,3V per deduzione. P.7 e p.8 richiedono chiarimenti sui rail. Il consumo pubblicato è tipico con pattern rosso, non massimo totale garantito. Non trasferire questi dati al codice VG acquistabile senza conferma. Pagine6/8 renderizzate e ispezionate; hash nel manifest.

## Richiesta tecnica pronta, non inviata

Per VIEWE: confermare l'MPN esatto venduto e la corrispondenza fra VG/WV/cartiglio; fornire datasheet corretto firmato, drawing quotato con tolleranze del vetro e inviluppo FPC ripiegato, connettore mating e revisione. Confermare tutti i rail e sequencing, livelli elettrici touch/QSPI, codice inizializzazione, consumo massimo per ogni rail con pattern/brightness/temperatura, temperature ammesse e vita/burn-in. Confermare stock, cinque campioni, prezzo, spedizione UE e continuità fornitura. Progetto: AMOLED touch1,75″, PCB custom, case preliminare circa52mm.

Per Huaxin: stessi dati e conferma corrispondenza ESHX/HX; accettazione di cinque campioni nonostante MOQ1000. Per LX/canali: datasheet originale dell'esatto MPN, touch installato, pinout, drawing e significato di51,5mm; confermare data effettiva per cinque unità. Non assumere intercambiabilità o disponibilità da un lead time generico.

## Disposizione proposta

Priorità di verifica documentale: VIEWE, poi Huaxin, poi LX. Nessuna scelta definitiva. Si può continuare con disegni concettuali e calcoli parametrici; schema elettrico e footprint pannello restano bloccati. Per chiudere O-16 servono identità univoca, documentazione consistente e fornitura confermata, poi campione per fit/assorbimento.

# Pannelli K0 — ricerca di secondo livello, 2026-10-24

Nessun acquisto o disponibilità riservata. VIEWE e Huaxin contattati con autorizzazione owner: docs/suppliers/SUPPLIER_CONTACT_LOG.md. Candidati nudi con touch, senza PCB elaborativo; una pagina di vendita non chiude O-16. I pannelli con codici diversi non condividono automaticamente il pinout.

| Candidato / canale | Evidenza commerciale | Evidenza tecnica | Esito |
|---|---|---|---|
| Huaxin ESHX0175Y003AMT003, pagina HX0175Y003AMT003 | E18: MOQ pubblico 1000, lead time 4–6 settimane. E32: Janie dichiara campioni a stock a USD10/unità; quantità5 e trasporto non confermati | E01 conservato; incongruenze già registrate | Disponibilità campioni dichiarata via email; identità codice/revisione e lotto5 da confermare |
| VIEWE ALL-UEOL018VG-RA31-A001A | E19: prezzo pubblico USD14,99 e vendita diretta; stock/lotto5/spedizione non confermati | E20 PDF V1.0 originale acquisito; copertina WV invece di VG, cartiglio ancora diverso | Candidato interessante per sourcing, non congelabile |
| LX Display LX175A3101CTP01 tramite Özdisan / Eurocoin | E21: stock0, MOQ1, lead time dichiarato4–5 settimane; E22 solo richiesta offerta | E21 indica 51,5×51,5 mm, E22 touch CST820; E29:datasheet originale esatto ottenuto successivamente, ma rail incoerenti | Alternativa da investigare, non sostituto equivalente |

E19 pubblica un pannello, non una devboard. Il suo prezzo implica USD74,95 per cinque soli pannelli prima di trasporto/imposte; non è un preventivo dell'assieme. E18 riporta una fascia prezzo troppo ampia per usarla nel budget. E21 non offre stock disponibile.

## Verifica critica E20 (PDF VIEWE)

Copertina: ALL-UEOL018WV-RA31-A001A; cartiglio p.6: ALL-UE015WV-RB24-A021A. La cover risulta48,16 mm, diversa da E01. P.5 contiene temperature non coerenti con la pagina commerciale. P.6 annota I2C1,8V; non collegare direttamente i segnali a3,3V per deduzione. P.7 e p.8 richiedono chiarimenti sui rail. Il consumo pubblicato è tipico con pattern rosso, non massimo totale garantito. Non trasferire questi dati al codice VG acquistabile senza conferma. Pagine6/8 renderizzate e ispezionate; hash nel manifest.

## Richiesta tecnica pronta, non inviata

Per VIEWE: confermare l'MPN esatto venduto e la corrispondenza fra VG/WV/cartiglio; fornire datasheet corretto firmato, drawing quotato con tolleranze del vetro e inviluppo FPC ripiegato, connettore mating e revisione. Confermare tutti i rail e sequencing, livelli elettrici touch/QSPI, codice inizializzazione, consumo massimo per ogni rail con pattern/brightness/temperatura, temperature ammesse e vita/burn-in. Confermare stock, cinque campioni, prezzo, spedizione UE e continuità fornitura. Progetto: AMOLED touch1,75″, PCB custom, case preliminare circa52mm.

Per Huaxin: stessi dati e conferma corrispondenza ESHX/HX; accettazione di cinque campioni nonostante MOQ1000. Per LX/canali: datasheet originale dell'esatto MPN, touch installato, pinout, drawing e significato di51,5mm; confermare data effettiva per cinque unità. Non assumere intercambiabilità o disponibilità da un lead time generico.

## Disposizione proposta

Priorità attuale di chiarimento: Huaxin, che ha risposto con prezzo campioni, poi VIEWE e LX. Nessuna scelta definitiva. Si può continuare con disegni concettuali e calcoli parametrici; schema elettrico e footprint pannello restano bloccati. Per chiudere O-16 servono identità univoca, documentazione consistente e fornitura confermata, poi campione per fit/assorbimento.

## Approfondimento LX — E29

Acquisito il PDF originale LX175A3101CTP01 Ver1.0 del10-12-2024, distribuito da Özdisan. Identità coerente con pagina del produttore LX; tavole fisiche p.5/7/8 renderizzate e ispezionate. La disponibilità resta stock0 del canale E21, non fornitura confermata.

Il documento non risolve il freeze elettrico: tabella absolute maximum e operating riportano ELVDD/ELVSS incompatibili, mentre il pinout descrive un ingresso VBAT3,7–4,5V e VCI3,3V. Non applicare i valori della tabella come schema pronto. Il disegno conferma cover51,5±0,05mm: con parete1mm e gioco0,3mm ipotetici, minimo aritmetico worst-case54,15mm prima delle tolleranze case. AØ52 non entra con quelle ipotesi. Nessuna deroga dimensionale ulteriore implicita.

Il PDF esatto e la pagina produttore migliorano la tracciabilità rispetto alla precedente indicazione «datasheet non ottenuto», ma servono chiarimenti e campione. Nessun pinout trasferito ad altri pannelli31pin.

## Risposte ricevute — 24 ottobre 2026

E32: Janie (Huaxin) dichiara campioni disponibili a USD10/unità e chiede l’indirizzo di spedizione per calcolare il trasporto. Per cinque pezzi il solo prodotto sarebbe USD50 se prezzo e quantità fossero confermati; non è un preventivo totale, prenotazione o ordine. Nessun lead time specifico o chiarimento elettrico fornito.

Il PDF allegato è ESHX0175Y003AMT003 V0, 2024-10-11. Confrontando entrambi tramite `pdftotext -layout`, l’unica differenza testuale rilevata è LCD MODULE → OLED MODULE in copertina. Le incongruenze elettriche/termiche restano; nessuna revisione corretta acquisita. I file PDF hanno hash differenti e sono conservati separatamente. Il subject/metadata del modulo indica erroneamente6,39″; testo della nostra richiesta e risposta di Janie indicano1,75″. Chiedere conferma esplicita del codice esatto e della configurazione touch prima di qualsiasi scelta.

E33: Jessica, Buyer Service Department Ecer, chiede quantità, applicazione, requisiti, budget e telefono/WhatsApp. È un contatto della piattaforma, senza offerta, stock o nuovo documento tecnico; non trattarlo come conferma indipendente del produttore. Quantità5, uso prototipo e requisiti sono già disponibili nella richiesta iniziale. Nessun budget vincolante o recapito telefonico fornito.

## Aggiornamento Huaxin — 25 ottobre 2026 (E53/E54)

La risposta WhatsApp fornita dall'utente supera le precedenti riserve su quantità/prezzo/trasporto: cinque moduli ESHX0175Y003AMT003 on-cell a USD10, UPS USD50, totale dichiarato USD100 e partenza entro2–3giorni dal pagamento. Non include una dichiarazione esplicita degli oneri importazione/IVA o della revisione campioni. Wiring V1 e sequenze QSPI/MIPI ricevuti; pacchetto touch identifica CST9217 ma resta da chiarire la relazione fra TP-VCC3,3V e segnali di test1,8V. Tre FPC di conversione sono opzionali e con codici diversi. Analisi, fonti, mappa pin documentale, limiti e hash in [HUAXIN_RESPONSE_2026-10-25](../../evidence/HUAXIN_RESPONSE_2026-10-25.md). Nessun datasheet integralmente rettificato, protocollo touch runtime o connettore mating confermato: O-16/schema/footprint rimangono aperti.

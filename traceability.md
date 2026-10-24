# Tracciabilità K0 — revisione 0.1

Data 2026-10-24. Fonte normativa SPEC_V0.2.md + DECISIONS.md (requisiti risultanti v0.2.1).
Generato da scripts/build_k0_registers.py. Gli sheet sono destinazioni pianificate: non esiste ancora un progetto KiCad.
«CANDIDATO» non significa selezionato o validato. Nessun test T è stato eseguito.

| ID | Requisito / decisione | Origine | Verifica prevista | Componente / sheet correlato | Stato | Prova disponibile / mancante |
|---|---|---|---|---|---|---|
| PR-001 | Supportare **solo OBD-II su CAN, ISO 15765-4**; nessun protocollo legacy K-Line/J1850. | SPEC §1.2 | T-004, T-008 | 01_OBD_INPUT / 04_CAN_PHY / firmware | CANDIDATO | CAN_POWER_TEST_PLAN.md; E03/E07; nessun test |
| PR-002 | Elettronica alimentata esclusivamente da OBD-II, destinata **esclusivamente a veicoli con impianto elettrico nominale a 12 V**; il funzionamento continuo su impianti nominali a 24 V **non è supportato**. | SPEC §1.2 | T-001, T-002, T-003 | 02_AUTOMOTIVE_POWER | BLOCCATO | O-07; nessun dimensionamento protezioni |
| PR-003 | Interrogare e rendere disponibili tutti i **PID OBD-II standard supportati dal veicolo**, non parametri proprietari. | SPEC §1.2 | T-008 | firmware catalogo PID | BLOCCATO | O-01; discovery proposta, non implementata |
| PR-004 | Sola lettura diagnostica: sono ammesse richieste diagnostiche necessarie a ottenere letture; proibite cancellazioni, attuazioni e scritture/stati modificabili. | SPEC §1.2 | T-008 + test allowlist negativi | firmware allowlist centrale | DA IMPLEMENTARE | SPEC §5.2; CAN_POWER_TEST_PLAN.md |
| PR-005 | Veicolo del primo collaudo: **Volkswagen Polo V/6R, 2011, 1.2 TDI, 75 CV, 3 cilindri**. | SPEC §1.2 | T-005, T-006, T-007 | banco e Polo effettiva | DA VERIFICARE | Nessun accesso alla vettura; nessun log |
| PR-006 | Display acceso con quadro acceso; resta acceso se motore spento ma quadro acceso. | SPEC §1.2 | T-005 | 04_CAN_PHY / power / stato IGN | BLOCCATO | HW-IGN / O-18 |
| PR-007 | Gauge lasciabile collegato per settimane; riaccensione automatica senza intervento umano. | SPEC §1.2 | T-006, T-007 | power always-on / wake | BLOCCATO | O-04/O-06/O-18 |
| PR-008 | Porta **USB-C nascosta**, destinata solo a sviluppo/manutenzione e recupero firmware; nessuna alimentazione esterna per l'uso normale. | SPEC §1.2 | recovery e matrice USB/OBD | 07_TEST_SERVICE | BLOCCATO | O-03; piano backfeed in CAN_POWER_TEST_PLAN.md |
| PR-009 | **Display circolare AMOLED 1,75 pollici**, touch, collegato tramite FPC a **un PCB completamente custom**. | SPEC §1.2 | drawing + sourcing + T-010 | 05_AMOLED_TOUCH | BLOCCATO | E01 respinto; MECHANICAL_K0.md; O-16 |
| PR-010 | Un tap in qualsiasi posizione sul display passa alla dashboard successiva. | SPEC §1.2 | T-010 | touch / firmware UI | DA IMPLEMENTARE | SPEC §6.1; nessun prototipo |
| PR-011 | Fino a **3 dashboard** e fino a **5 misurazioni per dashboard**. | SPEC §1.2 | T-010, T-011 + limiti 3x5 | UI / validazione firmware-app | DA IMPLEMENTARE | SPEC §6.1 |
| PR-012 | **Cavo OBD-II di 2 m**, con connettore removibile **lato display**; non usare due schede funzionali separate. | SPEC §1.2 | T-004, T-017 | 01_OBD_INPUT / cavo / presa gauge | BLOCCATO | MPN e cablaggio da verificare; O-21 |
| PR-013 | Montaggio magnetico. Limite dimensionale richiesto: **max 1,75 pollici**, interpretazione fisica esatta da chiarire. | SPEC §1.2 | fit quotato + prova montaggio/RF | case / magnete / PCB | BLOCCATO | DEC-001; MECHANICAL_K0.md; O-22 |
| PR-014 | Luminosità regolabile da app sia manualmente sia automaticamente con **sensore di luce ambientale**. | SPEC §1.2 | T-012 | 06_LIGHT_SENSOR / firmware / app | CANDIDATO | E06; curva O-12 non fissata |
| PR-015 | Fornitore PCBA candidato: **JLCPCB**, ma non definitivo. | SPEC §1.2 | T-018 | produzione | DA VERIFICARE | E10; RFQ non inviata |
| PR-016 | Prima tiratura: **5 unità**; obiettivo di spesa **40–70 EUR per unità assemblata**. | SPEC §1.2 | T-018 | BOM / lotto 5 | BLOCCATO | Nessun preventivo completo; O-19 |
| PR-017 | Output produttivo desiderato: **PCB assemblato con display e cavo OBD installati**. | SPEC §1.2 | T-018 + accettazione assieme | produzione / istruzioni FPC-cavo | BLOCCATO | O-17; SUPPLIER_RFQ_DRAFT.md |
| PR-018 | Gauge pienamente operativo senza iPhone; iPhone solo per configurazione, sincronizzazione dei dati previsti e aggiornamento. | SPEC §1.2 | T-013 | firmware autonomo / storage | DA IMPLEMENTARE | O-14 influenza storage, non autonomia UI |
| PR-019 | **Aggiornamento firmware OTA via BLE**. | SPEC §1.2 | T-015 | BLE / bootloader / flash | CANDIDATO | E02/E08; BLE transport e rollback non implementati |
| PR-020 | Associazione **esclusiva a un iPhone per volta**. | SPEC §1.2 | T-014 | BLE ownership / app | BLOCCATO | O-08 |
| PR-021 | App con **account e sincronizzazione cloud**, esclusivamente **iOS / Swift**, soltanto in italiano. | SPEC §1.2 | test app/account/offline | app Swift / backend | DA SPECIFICARE | O-13/O-23; fuori implementazione HW K0 |
| PR-022 | App non espone una schermata live: è un'app di configurazione; tuttavia registra sessioni/storici/log nell'app. Meccanismo di trasferimento da definire. | SPEC §1.2 | T-016 | storage / BLE log / app | BLOCCATO | O-14; nessuna garanzia storico definita |
| PR-023 | Dashboard con **layout completamente personalizzabile** (entro i limiti 3×5). | SPEC §1.2 | T-011 + editor | schema config / UI / app | BLOCCATO | O-10 |
| PR-024 | **Obiettivo di 5 Hz** per misurazioni rapide, compatibilmente con ECU, CAN, protocollo e numero di PID. | SPEC §1.2 | T-009 | scheduler / CAN | DA VERIFICARE | O-09; solo stime illustrative |
| PR-025 | Dato non disponibile: mostrare `—` e indicatore di indisponibilità, non zero né valore precedente. | SPEC §1.2 | timeout/stale/error injection | firmware catalogo / UI | DA IMPLEMENTARE | SPEC §5.3; nessun test eseguito |
| PR-026 | Dashboard predefinite al primo avvio con parametri standard **solo se supportati**. | SPEC §1.2 | discovery simulata + primo boot | firmware default dashboard | DA IMPLEMENTARE | O-24; no PID preteso supportato |
| O-01 | Scope preciso di «tutti i PID standard»: solo Service 01 oppure anche Service 02/09, DTC lettura? | SPEC §11 (P0) | decisione owner + test T-008 | firmware diagnostico | APERTA P0 | Scope Service 01 proposto, non approvato |
| O-02 | Max 1,75": diametro esterno finito, diametro PCB o area schermo? Tolleranza in millimetri? | SPEC §11 (P0) | decisione owner + CAD assieme | case / PCB / display | INTERPRETAZIONE CHIUSA; DETTAGLI APERTI | DEC-001: Ø esterno 44,45 mm; tolleranze/inviluppo plug da fissare |
| O-03 | USB-C alimenta temporaneamente il PCB per recovery senza OBD o richiede OBD collegato? Come prevenire backfeed? | SPEC §11 (P1) | decisione owner + test backfeed | 07_TEST_SERVICE | APERTA P1 | Due opzioni in CAN_POWER_TEST_PLAN.md |
| O-04 | Assorbimento massimo consentito in standby misurato su impianto nominale a 12 V, e dopo quanti minuti dal quadro OFF? | SPEC §11 (P0) | approvazione target + T-006 | 02_AUTOMOTIVE_POWER | APERTA P0 | 100 µA solo proposta; nessuna misura |
| O-05 | Budget di potenza/temperatura accettabile in funzionamento. | SPEC §11 (P1) | power budget + termica | power / display / MCU | APERTA P1 | Esempio 2 W non budget del prodotto |
| O-06 | Tempo di wake e timeout standby; comportamento in caso di CAN attivo con quadro OFF. | SPEC §11 (P0) | decisione owner + T-007 | wake / stato firmware | APERTA P0 | Nessun limite temporale approvato |
| O-07 | Temperature e profili automotive; vibrazione, umidità, esposizione solare, grado IP. | SPEC §11 (P1) | profili approvati + T-002/T-003 | protezioni / case / componenti | APERTA P1; BLOCCA K0 | Screening proposti; nessuna conformità |
| O-08 | Esclusività pairing, factory reset, riassegnazione owner e iPhone perso. | SPEC §11 (P1) | decisione owner + T-014 | BLE / app / ownership | APERTA P1 | Nessuna policy approvata |
| O-09 | 5 Hz vale per quanti PID contemporanei e quale tolleranza quando ECU lenta? | SPEC §11 (P1) | criteri owner + T-009 | scheduler | APERTA P1 | 1/5/15 PID da misurare |
| O-10 | Specifica tecnica «layout completamente personalizzabile». | SPEC §11 (P1) | decisione owner + T-011 | layout editor / schema | APERTA P1 | Nessun modello approvato |
| O-11 | Dashboard configurata per veicolo A usata su veicolo B con PID non disponibile. | SPEC §11 (P2) | decisione owner + test cambio auto | catalogo / UI | APERTA P2 | PR-025 resta obbligatorio per dato indisponibile |
| O-12 | Brightness auto/manuale: logica precedenze, curva, limiti e override. | SPEC §11 (P2) | decisione owner + T-012 | ALS / brightness | APERTA P2 | Curva, priorità e limiti da fissare |
| O-13 | Provider cloud, account, autenticazione, policy privacy e offline, retention storico. | SPEC §11 (P1) | requisiti backend/app | cloud / account | APERTA P1 | Nessuna scelta provider |
| O-14 | Origine e permanenza dei log se iPhone non è connesso: storage locale sul gauge oppure storico solo in app quando connessa. | SPEC §11 (P0) | decisione owner + T-016 | storage / memoria / log | APERTA P0 | Buffer locale o solo app connessa da decidere |
| O-15 | Distribuzione firmware e recovery/security policy. | SPEC §11 (P2) | decisione owner + T-015 | bootloader / distribuzione firmware | APERTA P2 | E08 solo capacità piattaforma |
| O-16 | Pannello AMOLED nudo realmente approvvigionabile con FPC/pinout e vincoli dimensionali. | SPEC §11 (P0) | datasheet corretto + campione + offerta | 05_AMOLED_TOUCH / meccanica | APERTA P0 | E01 respinto; issues/K0-001_BLOCKERS.md |
| O-17 | Chi monta connettore lato gauge, il cavo e il display e a che prezzo? | SPEC §11 (P0) | conferma scritta + T-018 | fornitore assieme | APERTA P0 | Bozza RFQ pronta, non inviata |
| O-18 | Meccanismo verificabile per «quadro acceso», senza dipendenze OEM. Quale compromesso è ammissibile? | SPEC §11 (P0) | HW-IGN: log CAN/corrente/ground truth | wake / CAN / power | APERTA P0 | Piano prove pronto; nessun test |
| O-19 | Budget 40–70 EUR include IVA, trasporto, attrezzaggio, cavo/display e montaggio o solo BOM+PCB? | SPEC §11 (P1) | decisione owner + preventivo completo | economia lotto | APERTA P1 | Perimetro imposte/trasporto non approvato |
| O-20 | Il prodotto è prototipo privato o sarà venduto? Impatta prove EMC, RED/CE e conformità automotive. | SPEC §11 (P1) | decisione destinazione prodotto | piano conformità | APERTA P1 | Privato/commerciale non fissato |
| O-21 | Connettore removibile lato gauge: orientamento, sporgenza e trattenuta meccanica. | SPEC §11 (P1) | drawing presa/plug + T-017 | 01_OBD_INPUT / case | APERTA P1 | MPN non selezionato |
| O-22 | Tipo magnete/supporto, geometria e distanza antenna BLE. | SPEC §11 (P1) | drawing + prove tenuta/RF | magnete / antenna | APERTA P1 | Nessun materiale o geometria selezionati |
| O-23 | Target versione minima iOS, provider login, gestione account. | SPEC §11 (P2) | requisiti app | iOS / account | APERTA P2 | Versione iOS e login da decidere |
| O-24 | Predefinite: 1 o 3 dashboard, metriche e layout iniziali quando supportati. | SPEC §11 (P2) | decisione owner + test primo boot | dashboard default | APERTA P2 | Solo PID scoperti supportati |
| O-25 | Supporto nominale 24 V escluso V1 | SPEC cronologia v0.2 e §11 | Revisione requisiti e BOM | power / scope | CHIUSA DAL PROPRIETARIO | SPEC v0.2; transitori >24 V restano da proteggere |

## Requisiti derivati senza ID originario

Queste righe integrano, senza sostituire, le 51 voci PR/O sopra. ID D-K0 locali al registro.

| ID locale | Origine | Verifica | Area | Stato / prova |
|---|---|---|---|---|
| D-K0-01 | SPEC §2/4: Unico PCB custom, FPC non gateway | review architettura | tutti gli sheet | DA VERIFICARE; nessun test eseguito |
| D-K0-02 | SPEC §2.1: Nessuna terminazione extra; cavo 2 m | T-004 / SI / EMC | 01_OBD_INPUT / 04_CAN_PHY | DA VERIFICARE; nessun test eseguito |
| D-K0-03 | SPEC §3.1: Protezione transienti, reverse, corto, hot plug | T-002/T-003 | 02_AUTOMOTIVE_POWER | DA VERIFICARE; nessun test eseguito |
| D-K0-04 | SPEC §3.1: Backfeed USB e rail periferiche | matrice quattro alimentazioni | 02 / 07 | DA VERIFICARE; nessun test eseguito |
| D-K0-05 | SPEC §3.2: Nessun polling in standby; IGN non da tensione sola | T-005/T-006/T-007 | CAN / wake | DA VERIFICARE; nessun test eseguito |
| D-K0-06 | SPEC §4.1: Fit quotato, RF e magneti | CAD e test RF | case / 03 / 05 | DA VERIFICARE; nessun test eseguito |
| D-K0-07 | SPEC §5.1/5.3: ISO-TP, ECU multiple, timeout, bus-off e catalogo | T-008 + fault injection | firmware | DA VERIFICARE; nessun test eseguito |
| D-K0-08 | SPEC §6.2: Config versionata, validazione, rollback | T-011 | firmware / app | DA VERIFICARE; nessun test eseguito |
| D-K0-09 | SPEC §7.1/7.4: Autenticazione, antireplay, OTA firmato e recovery | T-014/T-015 | BLE / flash / USB | DA VERIFICARE; nessun test eseguito |
| D-K0-10 | SPEC §8: Watchdog, termica, ambiente e tenuta magnetica | prove da definire in O-05/O-07/O-22 | sistema | DA VERIFICARE; nessun test eseguito |
| D-K0-11 | SPEC §10: ERC/DRC, review indipendente e package riproducibile | gate K2–K4 | KiCad / CI | DA VERIFICARE; nessun test eseguito |
| D-K0-12 | SPEC §13: Nessun ordine con P0 aperti senza deroga | review gate e autorizzazione | produzione | DA VERIFICARE; nessun test eseguito |

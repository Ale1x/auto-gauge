# CAN, alimentazione e prove K0 — proposta non congelata

Data 2026-10-24; requisiti SPEC v0.2 + DEC-001. Riferimenti E02–E09, E16–E17 in evidence/SOURCES.md. Nessuna misura hardware eseguita.

## CAN e cavo

Architettura candidata: TWAI del MCU + transceiver esterno TCAN1043A-Q1, CAN classico. Serve rail VCC 5 V e VIO compatibile 3,3 V; il regolatore 3,3 V e il sequenziamento restano da selezionare. Capacità CAN FD del transceiver non estende lo scope del controller o del prodotto. Validare tutte le combinazioni 11/29 bit × 250/500 kbit/s su simulatore T-008.

Avvio: partire in ascolto senza trasmissione; identificare bitrate su traffico valido quando possibile. Se non esiste traffico, non è garantita identificazione passiva. Autodetection diagnostica con trasmissione solo nel banco/stato attivo controllato autorizzato; non scandire bitrate trasmettendo su rete ignota. Implementazione dello stack conforme richiede fissare revisione ISO 15765-4/ISO-TP e catalogo PID standard prima di dichiarare conformità.

Proposta Service 01, subordinata a O-01: discovery pagina 00 e successive pagine a passi di 0x20 solo quando il bit di continuazione della ECU lo indica; capability e dati separati per ECU. Allowlist centralizzata dei servizi e PID ammessi, mai frame raw arbitrari da app. Vietati Mode 04, attuazioni e scritture. ISO-TP: bounded buffer, timeout, sequence counter e flow control verificati, anche risposte multiple. Gli header di indirizzamento, padding, tempi e ID ECU saranno fissati da documentazione normativa e fixture; nessun PID OEM per dedurre IGN.

Pinout preliminare da SPEC §2.1: 6 CAN-H, 14 CAN-L, 16 B+, 4/5 masse con strategia da progettare; altri pin non cablati senza requisito. Plug J1962 Type A candidato, MPN e drawing ancora aperti. La famiglia E16 è un punto di sourcing, non un plug già verificato. Non collegare alla cieca chassis/signal ground.

Cavo 2 m: coppia CAN intrecciata, ritorno potenza e sezione dimensionati, contatti bloccabili e strain relief. Non usare il solo nome OBD per dedurre twist, impedenza o pinout del cavo acquistato. Nessuna terminazione aggiuntiva da 120 Ω nel gauge. ESD CAN presso la presa; TVS e common-mode choke solo dopo valutazione capacità/clamp e SI.

Esempio di stima, non validazione: con propagazione ipotizzata 5 ns/m, il ramo 2 m vale 10 ns per tratta e 20 ns andata/ritorno; rispetto a 2 µs/bit a 500 kbit/s questo è 1%. Il rapporto non cattura riflessioni/edge-rate, carico e topologia del gateway: non prova che lo stub sia accettabile. Misurare differenziale e common-mode agli estremi, ringing, margine di campionamento, error frame e bus-off su traffico e carico realistici (T-004).

Budget interrogazioni esemplificativo: 5 PID visibili × 5 Hz = 25 richieste/s; 15 × 5 Hz = 75 richieste/s. Ipotizzando 2 frame da 160 bit per transazione (stima cautelativa, non limite normativo), sarebbero 8/24 kbit/s, cioè 1,6/4,8% a 500 kbit/s, 3,2/9,6% a 250 kbit/s, prima di ECU multiple, ISO-TP, retry, arbitraggio e traffico preesistente. Tempo ECU spesso dominante. Priorità PID visibili, rate ridotti per altri, timestamps monotoni e stato STALE/TIMEOUT con «—». T-009 deve registrare rate effettivo e latenza con 1, 5, 15 PID; soglie O-09 aperte.

## Alimentazione 12 V e protezioni

Proposta di catena da dimensionare: protezione fusibile vicino al prelievo B+ del cablaggio, senza seconda scheda funzionale → presa gauge → TVS/filtro coordinati → protezione polarità/OV/OC con MOSFET → buck automotive 5 V → rail 3,3 V e rail specifici del display. Il fusibile veicolo non sostituisce la protezione del cavo sottile. Correnti, tensioni, energia e dissipazione di ogni elemento restano da calcolare prima dello schematico finale.

LM5164-Q1 è solo un buck candidato: il suo rating 100 V non protegge connettori, condensatori, transceiver o controller di protezione. Sotto 6 V di ingresso non garantisce regolazione: comportamento durante crank da decidere. LM74900-Q1 richiede MOSFET esterni e verifica della corrente sempre presente; le modalità del componente non vanno sommate/interpretate senza schema. Nessun supporto nominale 24 V introdotto.

Profili ingegneristici da sottoporre a O-07, NON conformità dichiarata:

| Evento su impianto nominale 12 V | Proposta iniziale / verifica necessaria |
|---|---|
| Esercizio | Caratterizzare a 12 e 14,4 V; finestra UV/OV ancora da approvare |
| Crank | Screening con caduta a 6 V per 1 s; decidere reset consentito o mantenimento; non è una procedura ISO completa |
| Polarità inversa | Screening -14 V per 60 s con sorgente limitata; accettazione e procedura da approvare |
| Load dump soppresso | Valutare 35 V per 400 ms come scenario di progetto; waveform, Ri e ripetizioni da fissare con laboratorio |
| Load dump non soppresso | Non coperto da questa proposta; nessuna esclusione autorizzata. Valutare separatamente severità, energia e componenti |
| ISO 7637-2 impulsi 1/2a/2b/3a/3b | Definire edizione, livelli, Ri, fronti, durate, polarità, intervallo e ripetizioni; senza questi non c'è dimensionamento TVS valido |
| Hot plug/short | Inrush, rimbalzo contatti, corto sul cavo/carico e intervento protezioni da misurare |
| EMC/ESD | Piano condotte/radiate, ESD su prese/case e CAN da definire; test componente non è test assieme |

I numeri sopra sono H proposti da approvare, non limiti del prodotto né test passati. Prima di K1 verificare che Vclamp(I,T) + overshoot sia inferiore al limite del componente più debole con margine e che energia TVS e SOA dei MOSFET reggano l'intero impulso. Definire classe funzionale ammessa durante/dopo ogni evento.

Potenza attiva: P_in = somma(P_rail/efficienza_percorso) + perdite protezioni; corrente ingresso = P_in/V_ingresso. Esempio puramente parametrico: 2 W al carico con efficienza aggregata 85% richiedono circa 196 mA a 12 V e dissipano almeno 0,353 W nei convertitori. Non è un power budget del prodotto: mancano massimi display, BLE/MCU e condizioni termiche. Verificare white-screen, luminosità massima, OTA, CAN dominante, minimo ingresso e massima temperatura.

Standby: misurare l'intero ingresso B+ compresi transceiver wake, alimentazione always-on, protezioni, partitori, TVS, leakage USB/FPC, pull-up e wake spurii. Display/MCU spenti non implicano corrente nulla. Dimensionare partitori disinseribili e verificare retroalimentazione attraverso GPIO.

Sensibilità 4 settimane (672 ore), solo consumo aggiuntivo del gauge: 50 µA → 0,0336 Ah; 100 µA → 0,0672 Ah; 500 µA → 0,336 Ah; 1 mA → 0,672 Ah. Proposta per discussione O-04: ≤100 µA all'ingresso a 12 V nello standby stabile; NON requisito approvato. Con wake spurii 200 mA per 5 s ogni ora, si aggiungono 278 µA medi: includerli nella misura, non solo la lettura a riposo.

## Wake e quadro: gate HW-IGN

Ipotesi: transceiver in sleep controlla evento CAN e INH abilita alimentazione MCU. Il MCU parte con TX inibito; qualifica lo stato passivamente. Solo uno stato ATTIVO verificato può autorizzare richieste diagnostiche. Su timeout o evidenza insufficiente torna a riposo senza polling. Il criterio per entrare in ATTIVO resta da dimostrare: questa macchina a stati NON soddisfa da sola PR-006/007.

Se il gateway non espone traffico utile o il traffico a quadro spento è indistinguibile, il rilevamento universale non è dimostrato. Batteria/alternatore sono indizi, non IGN. Nessun compromesso automatico sul comportamento.

Prove richieste, dopo bring-up sicuro sul banco: prima con simulatore e poi sulla Polo effettiva, acquisire ground truth del quadro con annotazione manuale sincronizzata e strumentazione esterna; non aggiungere un ingresso IGN al prodotto. Scenari: ON/motore OFF, avviamento, motore acceso, OFF con porte aperte/chiuse, lock/unlock, attività ritardata gateway, caricabatteria e riaccensione dopo riposo prolungato. Registrare CAN passivo, alimentazione, corrente media/picchi, latenza e falsi wake. Strumenti, veicolo e dati non sono disponibili in questa sessione. Vietate dichiarazioni di successo o compatibilità Polo.

## USB e recovery

O-03 aperta. Opzione A: USB solo dati, OBD necessario e VBUS usato solo per sensing protetto. Opzione B: USB può alimentare il dominio service tramite power mux con reverse blocking, senza alimentare B+ o cavo. In entrambi i casi proteggere dati/CC/VBUS, verificare stato spento e alimentazioni parziali. Esaminare D+/D-, GPIO e diodi ESD per percorsi di backfeed. Non basta non unire VBUS a B+.

Testare OBD solo, USB solo, entrambi e nessuno; misurare tensione/corrente su porta inattiva e B+, hot plug in entrambe le sequenze. Sicurezza OTA e configurazione non devono disabilitare la sola recovery disponibile: definire bootloader, firma, rollback e policy debug prima di programmare eFuse irreversibili. Nessuna eFuse programmata.

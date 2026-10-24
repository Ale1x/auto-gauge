# Revisione indipendente dello schema A0

Data: 24/10/2026. Revisore: subagente `review_hardware`, separato dall’autore dello schema. Revisione documentale e della connettività, senza modifiche ai circuiti, simulazione o misure.

**Esito: A0 rimane incompleto e non costruibile.** Nessuna nuova inversione di pin confermata nei componenti esaminati. I blocchi P0 già dichiarati sono sostanziali. Individuato un punto aggiuntivo condizionale su GPIO3 e futura policy JTAG. Questa revisione non approva K0, layout, componenti, provisioning o collegamento al veicolo.

## Materiale effettivamente esaminato

- Tutti gli otto `.kicad_sch`, letti come S-expression: foglio principale con sette figli, 97 simboli nei figli.
- `hardware/pin_matrix.csv`, `hardware/schematic_bom_review.csv`, netlist XML e report ERC gerarchico; confronto mirato di **53 collegamenti critici** di U201/U202/U203/U401/U701.
- `hardware/README.md`, `hardware/ERC_REVIEW.md`, `USB_RECOVERY_K0.md`, documenti di standby/potenza e `BUCK5_FEEDBACK_REVIEW.md`.
- Tabelle dei datasheet originali locali: E05 LM74900-Q1 Rev.C; E04 LM5164-Q1 Rev.C; E46 TPS6281x-Q1 Rev.K; E30 TPS2116 Rev.A; E03 TCAN1043A-Q1 Rev.F; E02 ESP32-S3-WROOM-1 v1.8. Riverificati anche i documenti ufficiali online LM74900, TPS2116, TCAN1043A e WROOM.

La revisione dei pin non comprende la selezione dei MOSFET, passivi, TVS o connettori ancora TBD, né la verifica geometrica dei land pattern. Non è stata rieseguita una verifica ottica completa di ogni tratto del disegno PDF. Nessuna conformità automotive o USB dimostrata.

## Riscontri prioritari

| ID | Priorità | Stato rispetto ai documenti precedenti | Evidenza e conseguenza | Azione di chiusura |
|---|---|---|---|---|
| HR-01 | P0 | Già dichiarato; confermato | U201.6 EN è su `PROT_MID`; U401.10 VSUP e U202.2 VIN dipendono da `AON_PROTECTED`. Il guasto del bypass SLEEP può togliere alimentazione al transceiver e il segnale INH necessario alla ripartenza. Nessun reset indipendente del controller è disegnato. | Progettare recupero dal fault e sequencing; dimostrare ripartenza senza scollegare il dispositivo e rispetto del budget standby. Il solo RC R217/C217 non chiude il guasto. |
| HR-02 | P0 | Già dichiarato; confermato | J701 VBUS è su `USB_VBUS_RAW`; U701.6 VIN2 su `USB_5V_PROTECTED_TBD`: non esiste il percorso serie USB completo. La recovery alimentata da USB non funziona nello schema A0 così disegnato. | Completare protezione, limite di corrente e inrush prima di unire funzionalmente i domini; verificare alimentazione USB e backfeed. Nessun ponticello. |
| HR-03 | P0 | Già dichiarato; confermato | D201, Q201/Q202, R201/R203–R206 e C204 restano non dimensionati; non esiste un inviluppo elettrico verificato per `AON_PROTECTED`, che alimenta anche VSUP CAN. I rating del solo buck non proteggono gli altri carichi. | Congelare profili di prova e limiti del nodo protetto, poi TVS/FET/SOA/OCP/UV/OV/inrush con margini e misure. |
| HR-04 | P0 | Già dichiarato; confermato | U202 ha rete di ripple R210/C208/C209 TBD; lo screening pubblicato non dimostra stabilità. U203 ha passivi iniziali e capacità effettive non verificate. | Dimensionare e verificare entrambi i convertitori in avvio, carico, ingresso minimo e alimentazione USB. Non risolvere l’ERC aggiungendo solo power flag. |
| HR-05 | P0 | Già dichiarato; confermato | I 31 pin di J501 sono aperti e il livello touch resta indefinito. La presenza dei GPIO QSPI non costituisce interfaccia elettrica del pannello. | Specifica corretta e coerente del pannello, rail, livelli, sequenze e FPC prima dei collegamenti. |
| HR-06 | P1 | Specificazione di un rischio già aperto | R402/R403 portano EN/nSTB a 0: questo favorisce **Standby**, non garantisce Sleep né INH spento. INH attivo mantiene il percorso principale; attività CAN a quadro OFF può risvegliarlo. | Implementare una HAL con sequenza di Sleep e gestione flag del TCAN, poi prove con rete attiva a quadro OFF. Non dedurre il target 100µA dallo stato GPIO o dal solo firmware. |
| HR-07 | P1 | Già dichiarato; confermato | USB può alimentare VIO/MCU, mentre VCC/VSUP CAN dipendono da OBD. Gli ingressi `USB_PRESENT_SAFE_TBD` e `OBD_PRESENT_SAFE_TBD` non hanno circuiti di sensing. | Verificare tutti gli stati di alimentazione parziale e rendere il consenso TX falso per default. Non trattare gli ingressi aperti come misure valide. |
| HR-08 | P1 condizionale | **Nuovo dettaglio da tracciare** | U301.15/GPIO3 è NC. Il modulo non fornisce un pull interno su questo strap JTAG. Con eFuse di fabbrica la selezione ignora GPIO3; con `EFUSE_STRAP_JTAG_SEL=1` e JTAG non disabilitati, lasciarlo flottante rende indefinita la sorgente di debug. | Prima del provisioning fissare la policy JTAG e l’eventuale bias esterno. Non dichiarare la NC universalmente valida per ogni futura configurazione eFuse. Non è un guasto di boot dimostrato nell’attuale stato di fabbrica. |

HR-01…HR-07 non sono nuove autorizzazioni o deroghe: rendono più precisa la chiusura degli OPEN esistenti. HR-08 non richiede alcuna programmazione eFuse in questa fase.

## Verifica dei blocchi

### Protezione LM74900-Q1

La mappa di U201 è coerente con E05 tabella5-1: A sul lato ingresso di Q201, C/VS sul nodo intermedio, OUT sul lato sorgente di Q202; CS− sul lato opposto dello shunt rispetto al nodo intermedio, CS+ tramite R202 da0,1Ω. ISCP è collegato a CS+. CAP ha C202 verso VS e VS ha C201 verso massa. Q201/Q202 risultano nella disposizione common-drain, con lo shunt interposto nel collegamento verso il secondo drain: nessuna inversione G/D/S confermata.

IMON lasciato aperto è consentito quando non usato. Il requisito di lasciare flottante il pad RTN è correttamente riportato; il simbolo non lo porta a massa e il footprint di U201 non è fissato. Un futuro footprint deve mantenere tale proprietà. Il pad termico del LM5164 segue invece una regola differente.

Il bypass SLEEP non è una sorgente per l’intero carico. HR-01 resta bloccante anche se tutti i pin sono formalmente connessi. Riferimento: [TI LM74900-Q1](https://www.ti.com/lit/ds/symlink/lm74900-q1.pdf), tabella5-1 e sezione8.4.2.

### Buck 5 V e 3,3 V

U202.1/EP9 a massa, VIN2 su AON, EN3 su controllo HV, BST7 con C206 verso SW8, RON4 verso massa: funzioni coerenti. L202 separa SW da `5V_OBD`; R208/R209 riportano l’uscita al feedback. Il tipo di ripple disegnato collega SW→RA→nodo, CA dal nodo a uscita e CB dal nodo a FB. I valori restano aperti, quindi la topologia non equivale a regolazione stabile.

U203.2 VIN e8 EN sono su `5V_SERVICE`; MODE1 e COMP7 a massa, GND4 a massa, SW3 sull’induttore, FB5 sul partitore, SS6 sul condensatore e PG9 sul pull-up. La scelta COMP=GND è compatibile con la modalità prevista; per la variante1A il minimo di capacità dipende dalla fascia di tensione e dalla compensazione. Servono capacità effettive da curve reali, non la somma dei valori nominali. Non sono stati validati footprint, perdite e termica. Riferimenti: [TI LM5164-Q1](https://www.ti.com/lit/ds/symlink/lm5164-q1.pdf), [TI TPS6281x-Q1](https://www.ti.com/lit/ds/symlink/tps62813-q1.pdf), tabelle5-1/8-2.

### Mux TPS2116 e USB

U701.2 e7 sono entrambi VOUT e collegati a `5V_SERVICE`; VIN1.3 e MODE.5 sono su `5V_OBD`; PR1.4 riceve R706/R707; VIN2.6 è sul dominio USB protetto mancante. ST.8 non usato è un’uscita open-drain lasciata aperta. Non è stata trovata un’inversione tra MODE e ST.

MODE collegato a VIN1 è coerente con la priorità automatica candidata. Il rapporto340k/100k dà circa4,4V per il riferimento nominale1V, ma il riferimento specificato0,92…1,08V porta già la soglia a4,048…4,752V prima della tolleranza dei resistori. La commutazione va verificata con la finestra reale di `5V_OBD` e le cadute del mux, anche durante USB+OBD e brownout. Reverse blocking non elimina la necessità di misurare transitori e leakage. Riferimento: [TI TPS2116](https://www.ti.com/lit/ds/symlink/tps2116.pdf), tabella5-1, sezione6.5 e modalità di funzionamento.

### CAN e stato del veicolo

U401 segue la mappa14 pin del TCAN1043A: TXD1, RXD4, VCC3 da OBD, VIO5 da MCU, EN6, INH7 HV, nFAULT8, WAKE9 e VSUP10 su AON, CANL12, CANH13, nSTB14. Il collegamento WAKE a VSUP è coerente con l’ingresso locale non usato. Assenti resistori fissi da120Ω nella distinta. I ritorni OBD4/5 rimangono separati, con link DNP.

Il comportamento a pin non alimentati è favorevole secondo il datasheet, ma non sostituisce la prova dell’assieme con VIO presente e VCC/VSUP assenti. La modalità Silent è differente da Standby e Sleep; il consenso a richieste Service01 richiede alimentazioni valide e una qualificazione dello stato veicolo, ancora non dimostrata. L’uscita INH non prova il quadro acceso. Riferimento: [TI TCAN1043A-Q1](https://www.ti.com/lit/ds/symlink/tcan1043a-q1.pdf), sezioni7.3.8/7.4 e tabella7-4.

### Modulo MCU e recovery

U301.28/29/30 (GPIO35/36/37) sono NC: non assegnati al pannello sul candidato N16R8. USB D−/D+ usa rispettivamente pin13/14, GPIO19/20; BOOT usa GPIO0/pin27, con pulsante a massa, e RESET agisce su EN/pin3. GPIO45 e46 non sono usati da periferiche e dispongono dei default interni previsti; la loro compatibilità va conservata nella futura policy del modulo e della recovery.

GPIO3 richiede la distinzione HR-08: il default eFuse lo ignora, mentre una selezione futura tramite strap deve definirne il livello. Non è giustificato un nuovo resistor value senza quella policy. Le eFuse non sono state modificate e la recovery sicura non è stata collaudata. Riferimento: [Espressif WROOM-1 v1.8](https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf), sezioni3/4 e tabella4-5.

## ERC ed evidenza riproducibile

Il report effettivo conserva 31 pin FPC aperti, 5 alimentazioni senza driver ERC e 2 etichette isolate: **36 errori e2 warning**. Non sono state inserite esclusioni. Il risultato0 del parser MCP è già documentato come falso riepilogo gerarchico; non è stato usato come criterio di accettazione.

SHA256 della netlist esaminata: `809d41393a73d603a51d57ca0559818eaaab92e308e3bcc3eed5331880c9aded`.

| Sorgente esaminata | SHA256 |
|---|---|
| `02_AUTOMOTIVE_POWER.kicad_sch` | `6b7b962d7e82749709989ca68684572619789b177ec91162c7689decde330d9b` |
| `03_MCU_MEMORY_BLE.kicad_sch` | `a8551399fbc706f976949c258419d6fbaeb76be6e31da40fe82c695788eff9e7` |
| `04_CAN_PHY.kicad_sch` | `a0ecdacf608de7470e34a84b80e5d3f10030e51008d239ed7e4fbc676e33c283` |
| `07_TEST_SERVICE.kicad_sch` | `b9ea4ea33d2217152c83194f0a91cb3b3bc3762381200c9b9116d3cf10d06ae5` |

La verifica mirata controlla corrispondenza di pin e dominio, non il comportamento analogico. Il confronto con il generatore non costituisce una seconda prova indipendente della correttezza delle sue ipotesi: l’evidenza esterna è la tabella originale del produttore.

## Ordine delle prossime chiusure

1. Alimentazione always-on e recupero fault/wake, insieme al limite standby e ai profili automotive.
2. Funzione serie USB e sensing affidabile dei domini, prima di autorizzare qualunque attività CAN da firmware.
3. Passivi e limiti dei buck; simulazione e prove prima dell’alimentazione dell’assieme.
4. Dati pannello/FPC e verifica meccanica; footprint reali e DFM solo dopo i dati mancanti.
5. Policy boot/JTAG/recovery e test con immagini firmate prima di qualsiasi provisioning irreversibile.

App e nucleo firmware possono avanzare con simulatori e HAL non attive. Nessun risultato software chiude questi test hardware.

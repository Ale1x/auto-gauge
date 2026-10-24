# Recovery USB alimentata — analisi K0

Data 2026-10-24. DEC-007 approva alimentazione temporanea USB per recovery senza OBD. L'uso normale resta alimentato esclusivamente da OBD, nominale12V. Nessun circuito definitivo progettato.

## Percorsi candidati

OBD → protezioni automotive → buck5V commutabile → ingresso1 mux; USB VBUS protetto → ingresso2 mux; uscita → dominio MCU/flash/service e regolatore3,3V. Un mux a bassa tensione non deve vedere B+ o transienti automotive. OVP/limite corrente USB e protezioni dati/CC da selezionare. Il dominio CAN deve restare inibito quando manca OBD; USB non autorizza a trasmettere sul veicolo.

Candidato da approfondire: TPS2116DRLR, mux con reverse blocking, range1,6–5,5V; datasheet Rev.A conservato. **Non è un componente definitivamente scelto.** La pagina commerciale indica un range termico diverso dalla tabella recommended conditions del PDF consultato, che arriva a+105°C: usare il dato del PDF come limite di studio finché TI non chiarisce variante/revisione. Stock/prezzo non confermati. Non dedurre dalle specifiche reverse blocking l'assenza assoluta di leakage o transitori di backfeed.

USB4105 di GCT è una famiglia di prese candidata: pagina produttore indica profilo3,31mm e lunghezza corpo7,35mm; opzioni di codolo/placcatura e drawing esatto da fissare prima del footprint. Nessun MPN completo congelato; il rating corrente della presa non autorizza il carico ad assorbire quella corrente dalla sorgente USB. CC, enumerazione/limiti correnti e inrush richiedono progetto secondo documentazione USB applicabile.

## Comportamento da verificare

| OBD | USB | Comportamento previsto da verificare |
|---|---|---|
| Assente | Assente | Nessun dominio alimentato; ingressi non retroalimentati |
| Presente, attivo | Assente | Gauge da OBD; VBUS della presa USB non alimentato dal gauge |
| Assente | Presente | MCU/flash/recovery disponibili; B+ del cavo non alimentato, nessuna richiesta CAN |
| Presente | Presente | Priorità sorgente da fissare; entrambe protette da corrente inversa, niente risveglio diagnostico per sola USB |
| Presente, quadroOFF | Assente | ≤100µA all'ingresso12V entro5min, senza BLE/logging che prolunghino l'attività |
| Presente, quadroOFF | Presente | Sessione service separata; consumo da USB e OBD da misurare separatamente, senza mascherare un fallimento dello standby normale |

O-03 chiusa nel caso d'uso, non nella validazione anti-backfeed. Se USB alimenta il MCU mentre VSUP CAN manca, isolare EN/nSTB/TXD/RXD e segnali di sensing da clamp/parasitic power. Verificare anche periferiche/display spente e alimentazioni parziali, non solo i due ingressi del mux.

USB recovery non richiede necessariamente display luminoso o logging: default proposto service ridotto, da validare. Secure boot/eFuse e policy download devono conservare un percorso di recupero compatibile con firmware firmato; non programmare eFuse irreversibili finché la procedura non è verificata. Recovery boot accessibile senza smontaggi distruttivi del case.

## Prove richieste prima di K1/K2 finali

Verificare tutte le righe della matrice con hotplug nei due ordini, tensione/corrente sulla porta inattiva e B+, inrush, brownout, rimozione alimentazione durante flash, recupero di immagine interrotta, GPIO a dominio spento e connessione USB con CAN collegato. I limiti di leakage/backfeed e di alimentazione USB vanno fissati nel piano di prova; nessun PASS derivato dal solo datasheet del mux.

Fonti: E30 TPS2116 PDF Rev.A; E31 GCT USB4105. Nessuna misura hardware eseguita.

## Vincoli ROM, sicurezza e accesso di recupero

E49/E50 distinguono USB-Serial/JTAG da USB-OTG DFU. La guidaDFU Espressif dichiara che SecureBoot o flash encryption disabilitano lo stackUSB-OTG inROM: non usare DFU come unica recovery dopo l’attivazione della sicurezza. LaFAQ Espressif distingue invece il downloadUSB tramite `idf.py flash` dal debugJTAG e da `dfu-flash`; non conclude che ogni configurazione eFuse o pacchetto cifrato sia recuperabile automaticamente.

Candidato da verificare: GPIO19D− eGPIO20D+ riservati al percorsoUSB-Serial/JTAG del modulo; accesso manuale aGPIO0/RESET nell’area service per entrare nel downloader anche se l’applicazione riconfiguraUSB o non parte. GPIO46 deve rispettare il livello richiesto perJointDownloadBoot (E02/E49). Nessun pin MCU è già assegnato nel progetto e nessuna eFuse modificata.

La guidaSecureBootv2 consente politiche differenti del ROMdownload: un’impostazione che lo disabilita permanentemente può eliminare la recovery voluta. Prima di fissare la policy: selezionare releaseSDK, revisione chip/modulo, stato eFuse e modalitàSecureDownload; definire chi firma/cifra l’immagine e quali operazioni restano disponibili. Non pubblicare chiavi o usare i file di chiave in repository. Nessuna chiave generata e nessuna provisioning eseguita.

La validazione richiede flash di immagine firmata valida, rifiuto immagine non valida, applicazione corrotta, interruzione in ogni fase, USBsola eOBD+USB, partizione OTA errata e perdita di alimentazione durante recovery. Verificare ripresa senza app funzionante, conservazione dati ammessa e assenza di richiesteCAN improprie. Usare unità dedicate per prove irreversibili dopo procedura approvata; il test non è stato eseguito.

Il bilancio elettrico USB è in MCU_RAIL_K0.md. Sorgente capace di0,5A sul3,3V e corrente effettivamente prelevabile daUSB sono condizioni differenti. Validare anche la faseROM e l’inrush; nessuna compatibilità universale con qualsiasi portaUSB è dichiarata.

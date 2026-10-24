# Registro decisioni approvate

## DEC-001 — 2026-10-24 — revisione requisiti v0.2.1

Origine: risposta del proprietario in questa chat: «Diametro esterno del dispositivo: 44,45 mm».

O-02 è risolta per l'interpretazione: **il diametro esterno finito massimo è 44,45 mm**. Si applica al dispositivo, non al solo PCB o all'area attiva del pannello. Il limite non autorizza aumenti di diametro né riduzioni del display PR-009. Tolleranze produttive e inclusione dell'inviluppo del connettore accoppiato vanno ancora esplicitate; non si assume una tolleranza positiva.

Questo addendum approvato integra SPEC_V0.2.md, conservata invariata. La revisione dei requisiti risultante è v0.2.1. Non è un'approvazione di K0, BOM o produzione.

## DEC-002 — 2026-10-24 — revisione requisiti v0.2.2

Origine: risposta del proprietario: «Bene, allora facciamolo leggermente più grande, non è un problema».

È approvato un aumento contenuto del diametro esterno rispetto a 44,45 mm, per permettere lo studio dell'assieme con display AMOLED touch nominale 1,75″. DEC-001 è superata **nel limite numerico**, mentre resta confermato che l'ingombro di interesse è quello esterno del dispositivo. PR-013 è aggiornato in questo senso; PR-009 resta invariato.

Il nuovo massimo numerico non è stato indicato dal proprietario. Ø52 mm è un'ipotesi ingegneristica iniziale, non un massimo approvato né un fit validato. Prima del freeze serviranno quote reali di pannello/FPC, pareti e tolleranze, connettori, magnete e RF. O-02 rimane aperta per il valore finale, tolleranze e inviluppo del plug accoppiato.

Questo addendum integra SPEC_V0.2.md e porta i requisiti risultanti a v0.2.2. Non approva K0, BOM o produzione.

## DEC-003 / DEC-004 / DEC-005 — 2026-10-24 — revisione requisiti v0.2.3

Tre decisioni approvate dal proprietario in questa chat:

- **DEC-003 — O-01:** «Solo PID live Service 01 (consigliato)». La V1 comprende solo PID live Service 01 standard supportati dal veicolo. Service 02/09 e lettura DTC non rientrano nel perimetro approvato. Restano vietati servizi di scrittura/attuazione e Mode 04. La compatibilità e i decoder richiedono comunque verifica.
- **DEC-004 — O-14:** «Buffer sul gauge e sincronizzazione successiva. Però senza intasare il gauge». È approvato il buffering locale con successiva sincronizzazione BLE; deve essere limitato e non sovraccaricare il dispositivo. Capacità, frequenza log, retention, comportamento a pieno e perdita ammessa non sono fissati. LOGGING_K0.md propone una soluzione circolare bounded: i suoi valori numerici restano H, non decisioni approvate.
- **DEC-005 — O-04 / parte O-06:** «Sì: 100 µA entro 5 minuti». Target massimo all'ingresso OBD a 12 V: 100 µA entro 5 minuti dal quadro OFF. Il limite è approvato, non misurato. Condizioni termiche, banda e picchi da precisare; latenza di riaccensione e rilevamento IGN restano aperti. Nessuna deroga per CAN attivo a quadro OFF.

Le decisioni integrano SPEC_V0.2.md e i precedenti addendum; requisiti risultanti v0.2.3. O-01 chiusa come decisione di scope; O-04 chiusa nel target e tempo, aperta nella prova/metodo; O-14 risolta nel meccanismo, aperta nei limiti quantitativi. Nessuna approvazione K0, PCB o ordine.

## DEC-006 / DEC-007 — 2026-10-24 — revisione requisiti v0.2.4

- **DEC-006 — O-14:** owner approva la policy proposta:2MiB fissi se disponibili dopo sizing OTA, registrazione fino a1Hz per PID, sovrascrittura dati più vecchi con segnalazione della perdita. Non è garantita una durata fissa: overhead, numeroPID e formato record la determinano. La lettura/UI può restare a5Hz ove supportato; il logger non deve rallentare i percorsi critici. Se2MiB non sono disponibili dopo sizing, riportare il conflitto; non ridurre OTA/recovery autonomamente.
- **DEC-007 — O-03:** owner approva alimentazione USB temporanea per recovery anche senza OBD collegato. USB non è alimentazione d'uso normale. Restano obbligatorie protezioni e verifica di backfeed in entrambe le direzioni.

Autorizzazione operativa: invio delle richieste già preparate a VIEWE, Huaxin, JLCPCB e PCBWay approvato, senza ordine o impegno di acquisto. Recapito di risposta fornito dall'owner registrato in SUPPLIER_CONTACT_LOG.md.

Queste decisioni integrano gli addendum precedenti; requisiti risultanti v0.2.4. Nessuna approvazione del gate K0 o produzione.

## DEC-008 — 2026-10-24 — revisione requisiti v0.2.5

Origine: alla domanda «Durante l’avviamento del motore, accetti che il gauge possa riavviarsi per una breve caduta di tensione, purché riparta automaticamente?» il proprietario risponde «Sì, riavvio automatico accettabile».

Durante l’avviamento motore è ammesso un riavvio del gauge dovuto a una breve caduta di alimentazione, con ripartenza automatica. Questa eccezione aggiorna PR-006 nel solo evento crank; non autorizza spegnimenti a quadro ON fuori da quell’evento, né richiede continuità del display durante la caduta. Non introduce un’alimentazione nominale24V.

O-07 è risolta solo per la classe funzionale durante il crank: reset ammesso, recupero autonomo richiesto. Livelli/durate del profilo, soglie brownout e latenza massima di recupero restano aperti e da verificare. Integrità configurazione/log/OTA, assenza di trasmissioni diagnostiche improprie al riavvio e protezioni restano da dimostrare.

Requisiti risultanti v0.2.5; SPEC_V0.2.md resta conservata invariata. Nessuna approvazione del gate K0, di componenti definitivi o produzione.

## DEC-009 — 2026-10-24 — autorizzazione operativa allo schema di revisione

Origine, richiesta esplicita del proprietario: «Procedi,non fermarti fin quando abbiamo lo schema kicad, fai tutti gli step necessari».

È autorizzato proseguire a pin assignment, progetto e schema KiCad modificabile per revisione ingegneristica, anche con i blocker K0 già documentati. Questa richiesta successiva prevale sulle precedenti condizioni operative che impedivano di creare lo schema. Non dimostra il superamento K0 né risolve dati tecnici mancanti: connessioni/valori non verificati devono restare marcati H/TBD/OPEN, senza dichiarare lo schema definitivo o pronto alla fabbricazione.

La revisione A0 non autorizza layout/routing PCB, ordini, eFuse irreversibili o collegamento alla vettura. I requisiti funzionali restano v0.2.5; si tratta di una variazione dell’ordine di lavoro, non di una deroga a protezioni, diametro, standby o comportamento IGN.

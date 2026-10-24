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

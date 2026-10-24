# Connettori e cavo — shortlist K0, 2026-10-24

Obiettivo: due metri di cavo, unico PCB custom, rimozione lato gauge. Nessun assemblaggio o acquisto effettuato. Fonti E24–E28 in evidence/SOURCES.md.

## Candidato principale da approfondire

Molex Micro-Fit 3.0 a sei posizioni: **0430200600** lato gauge, con agganci a pannello, e **0430250600** lato cavo con contatti femmina. Quest'ultimo evita pin maschi alimentati esposti sul cavo staccato, ma shrouding e accessibilità del B+ richiedono verifica. Servono terminali delle famiglie43031/43030, MPN esatti da scegliere con sezione/isolante/placcatura e attrezzaggio crimp. I soli housing non costituiscono una connessione completa.

E24/E25 identificano la famiglia e le due metà; E27/E28 mostrano indicazioni pubbliche di stock/prezzo. Non sono riservate, né conferme PCBA. Il drawing di famiglia E25 è consultabile online, ma il download locale non è riuscito: non sono state trascritte quote di taglio/inviluppo dal solo testo estratto. Serve verifica della tavola completa e del mating montato prima del CAD.

Il montaggio a pannello può richiedere un breve pigtail interno saldato sul medesimo PCB: nessuna scheda aggiuntiva. Inserimento housing, crimp, strain relief e tenuta al tiro devono essere quotati dal fornitore. Proteggere il PCB dal carico meccanico del cavo.

Sei posizioni permettono di mantenere separati B+, CAN-H, CAN-L e i due riferimenti OBD4/5 durante lo studio, lasciando una posizione non assegnata. Non è approvato il collegamento elettrico delle masse né il pinout fisico della presa. I due riferimenti non diventano automaticamente due ritorni di potenza in parallelo. Nessun pin OEM è aggiunto.

## Alternativa da confrontare

Header PCB ad angolo Molex **0430450400** a quattro posizioni, accoppiabile con housing43025 a quattro vie (E26). Riduce pigtail, ma vincola il montaggio al PCB e obbliga a decidere la strategia di massa prima di eliminare una linea. La pagina riporta 30 cicli di accoppiamento: verificare rispetto alla frequenza di rimozione desiderata. Non è una scelta definitiva; sealing e uso esterno al case non sono dimostrati.

Una presa industriale filettata compatta può essere un'ulteriore alternativa, ma non è stato identificato un MPN completo verificato con mating, tensioni transitorie e budget accettabili. Non inserirla nella BOM come componente selezionato.

## Cablaggio: requisiti per il fornitore

Specifica preliminare: lunghezza finita2m, coppia CAN intrecciata con impedenza/velocità documentate, conduttori B+/ritorno dimensionati, twist mantenuto vicino alle prese, guaina e strain relief adatti all'ambiente. Nessun120Ω nel gauge. Schermo e collegamento chassis da decidere dopo EMC. Plug veicolo J1962 Type A: famiglia E16 candidata, MPN e drawing del plug reale ancora aperti; non sostituire con presa vehicle-side.

Modello di caduta: R_loop = 2Lρ/A, ΔV = I·R_loop, P_cavo = I²R_loop; aggiungere contatti, fusibile e giunzioni. Esempio H con L=2m, A=0,22mm², ρ=0,0175Ω·mm²/m e I=0,5A: R_loop≈0,318Ω, caduta≈0,159V, perdite≈0,080W. È un modello a temperatura di riferimento: resistenza reale, derating termico e corrente massima non sono fissati. Sezione e fusibile si dimensionano anche per corto e inrush, non solo per questa caduta.

La protezione del cavo deve essere vicina al prelievo B+, senza secondo PCB funzionale; non affidarsi soltanto al fusibile del veicolo. Controllare continuità, assenza di corti, mapping di entrambe le estremità viste dal lato mating, polarizzazione, ritenzione, tiro e flessione. T-004/T-017 restano NON ESEGUITI.

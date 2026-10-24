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

Approfondimento: POWER_ENVELOPE_K0.md e relativo CSV riproducibile. Comprende caduta cavo e perdite, vincolo del bypass SLEEP/avvio del carico e DEC-008 (reset crank ammesso con ripartenza automatica). Nessuna sezione/fusibile o protezione congelata.

## Terminali esatti e attrezzaggio — approfondimento K0

Per la coppia6vie candidata, individuati terminali sciolti stagnati **0430310007 maschio** nel43020 e **0430300007 femmina** nel43025 (E36/E37). Sono candidati20–24AWG; non una scelta del cavo. ATS-638190000 Rev.G (E38) include entrambi con attrezzo63819-0000, sezione tabellare0,20–0,50mm², diametro isolante1,42–1,85mm e spellatura2,54–2,92mm. Confermare revisione vigente, filo reale, crimp e attrezzaggio del fornitore; non usare una pinza generica per deduzione.

Il modello di potenza include0,14mm², ma questo non rende tale sezione compatibile con questi terminali:20–24AWG è il campo documentato. Non convertire automaticamente0,22mm² in una crimpatura valida senza costruzione del filo e isolamento verificati. Il cavo deve essere scelto insieme al terminale; una sezione diversa richiede il suo codice appropriato.

Le pagine Molex riportano **30 cicli di accoppiamento** per i contatti candidati. Non limitare questo rischio all’alternativa PCB: interessa anche la coppia a pannello. Se il gauge viene scollegato spesso, la sua adeguatezza non è dimostrata. La rimovibilità richiesta non fissa da sola quanti cicli siano accettabili.

Sourcing E39: indicazione DigiKey232719 terminali maschi aUSD0,23 e422268 femmine aUSD0,20, osservazione indicizzata non riservata. Con cinque linee ipotetiche, housing0,40+0,51 più5×(0,23+0,20) =USD3,06 per coppia completa di soli housing/contatti, prima di scarti, crimp, pigtail, cavo, plugOBD, trasporto/imposte e lavoro. Con tutte e sei le posizioni popolate:USD3,49. Il quinto riferimento massa non è collegato in parallelo automaticamente e il pinout non è congelato.

## Confronto per rimozioni frequenti

Alternativa documentata E40: presa a pannello **Hirose HR30-6R-6P(71)** e plug cavo femmina **HR30-6P-6S(31)**, sei posizioni, saldatura e push-pull. Pagina plug:1000cicli,2A, -25…+85°C, contatti femmina ma finger protection dichiarata NO. Il rating non garantisce protezione del B+ accessibile né qualificazione dell’intero cablaggio; transitori, CAN, ingresso cavo e fit restano da verificare. Non trasferire IP del connettore al case completo.

E41, canale Mouser: presa indicizzata1344pezzi/USD8,04; plug stock0, prezzo espostoUSD15,38 e lead time26settimane. Somma solo connettoriUSD23,42, prima di cavo/saldatura/trasporto/imposte. Questa coppia è una riserva comparativa con sourcing sfavorevole; non viene scelta come sostituto né aggiunta come costo cumulativo alla BOM principale. Il suffisso di revisione non è intercambiabile per deduzione.

Disposizione corrente: mantenere Molex allo studio per il costo, con rischio cicli esplicito; nessuna approvazione di durabilità. Chiedere al fornitore assieme conferma processo crimp/cavo nella risposta già richiesta; evitare nuovi invii duplicati prima della risposta. DisegnoE25 raggiungibile dal motore web, download locale ancora fallito anche con timeout eHTTP1.1: nessun cutout o dimensione mating trasferita al CAD dal testo estratto, che presenta errori di tabella/unità.

## Plug OBD e cavo: ricerca di codici concreti

**E42 — KDS articolo33114, Stecker44:** kitJ1962 TypeA12V dichiarato dal venditore,16contatti, due semigusci e strain relief. Prezzo espostoEUR4,99 a quantità1–49 conIVA19%, disponibilità indicata2–4giorni; destinazioneItalia, aliquota/consegna e quantità5 non confermate. È un codice fornitore, non un MPN del produttore verificato. Mancano tavola quotata con vista mating, rating contatti/temperature e sezione cavo ammessa; candidato commerciale soltanto. La pagina del diverso36013/Stecker11 descrive una piccola PCB porta-pin e offre disegni: non trasferirli al33114 e non usare la sua costruzione per il requisito a singoloPCB senza verifica.

**E43 — Molex685031602:** pagina ufficiale identifica housing16vie con campo-10…+85°C; documento storico di prodotto esportato dal produttore riporta New Business Not Supported, mentre la pagina attuale espone solo informazioni limitate. Non interpretare tale discordanza come stock o continuità approvati. Nessuna variante TypeA completa con terminali/back-shell congelata; non è una sostituzione pronta.

**E44 — LAPP2170261, UNITRONIC BUS CAN:** due coppie0,22mm², diametro7,6mm e impedenza120Ω documentati; la scheda definisce installazione fissa e riporta tensione250V con nota «not for power applications». Non basta per approvare una coppia come alimentazione del gauge. Servono conferma del costruttore per corrente/impiego, isolamento/temperatura e fit nel pressacavo. Due coppie offrono quattro conduttori isolati: non risolvono automaticamente lo studio con riferimentiOBD4/5 distinti; la schermatura non è un quinto conduttore equivalente. Le due sezioni della scheda riportano anche temperature diverse: non estendere il rating dall’introduzione generica. Nessun cavo selezionato o acquistato.

Cavi precablati KDS esaminati: la categoria distingue TypeA eTypeB; alcuni modelli con twist esplicito sonoTypeB e3m, quindi non vanno sostituiti al requisitoTypeA/2m. Un TypeA3m con conduttoriAWG24 non prova da solo impedenza/twist e non costituisce un assieme2m finito. Il cavoNI193942-02 è dichiaratoTypeA2m senza terminazione, ma usaD-sub e costa daUSD154: utile come riferimento di banco, non soluzione nel budget del gauge.

Richiesta documentale aKDS in SUPPLIER_MESSAGES_READY.md §4 inviata viaGmail dopo estensione esplicita del consenso in chat; labelSENT e messaggio registrati in SUPPLIER_CONTACT_LOG.md. In attesa di risposta, nessuna conformità o fornitura confermata.

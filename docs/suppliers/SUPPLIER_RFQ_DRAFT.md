# Bozza richiesta fattibilità e preventivo — NON INVIATA

Data 2026-10-24. Destinatari candidati: JLCPCB, PCBWay; Seeed Fusion come ulteriore confronto. Il file è preparatorio: nessun contatto, upload o ordine effettuato. Un preventivo definitivo richiederà pannello/BOM/disegni risolti.

## Evidenze pubbliche

JLCPCB elenca lavorazioni manuali e servizi OLED/flashing/test soggetti a valutazione (E10). PCBWay propone OEM con box build e richiede file PCB, BOM, housing e istruzioni (E11). Seeed offre kitting e sourcing anche per cinque set, ma il montaggio finito è da confermare (E12). Non si inferisce accettazione di questo progetto da una pagina servizi.

## Testo da inviare dopo autorizzazione al contatto

Richiediamo studio di fattibilità per cinque gauge OBD-II 12 V con un PCB custom, pannello AMOLED touch nudo 1,75″ su FPC, cavo CAN intrecciato lungo 2 m, plug J1962 Type A e connettore removibile lato gauge. Diametro esterno leggermente maggiore di 44,45 mm autorizzato; ipotesi preliminare Ø52 mm, massimo finale da definire; pannello e assemblaggio non ancora congelati. Nessun ordine in questa fase.

Confermare per iscritto:

1. Accettate lotto di 5 assiemi completi e materiali a basso MOQ? Quali scorte extra/scarti minimi richiedete?
2. Potete procurare il pannello esatto con drawing/pinout e revisione approvati oppure accettate materiale in conto lavoro? Indicare MOQ, lead time, continuità fornitura e responsabilità danni/scarti.
3. Inserite e bloccate FPC, montate pannello/touch, adesivi e strain relief manualmente? Accettate FPC con componenti integrati e limiti di piega? Specificare controlli visivi/funzionali.
4. Procurate e assemblate cavo CAN 2 m con coppia intrecciata e sezione/pinout approvati, protezione cablaggio, plug Type A e presa/plug gauge? Eseguite test continuità, corto e trazione?
5. Potete montare USB-C e supporto magnetico, rispettare keep-out RF e disegno dell'assieme? Distinguere lavorazione case e montaggio accessori.
6. Confermate flashing, serializzazione e test rail/display/touch/ALS/BLE/CAN su fixture, tempi e NRE? Quali file e fixture servono?
7. Quotare separatamente PCB/stackup, SMT/THT, sourcing, pannello, cavo, connettori, montaggio manuale, magnete/case se incluso, collaudo, attrezzaggi, scarti, imballaggio, trasporto, imposte e dazi.
8. Indicare costo totale e per cinque unità, valuta, condizioni di pagamento, validità, resa e lead time. Obiettivo indicativo 40–70 EUR per unità; non costituisce prezzo accettato.
9. Elencare ogni esclusione, componente non montato, materiale fornito dal cliente e attività residua necessaria per consegnare l'assieme funzionante.

## Confronto economico da compilare

C_totale = componenti + PCB + PCBA + display + cablaggio/connettori + lavoro manuale + test + attrezzaggi + trasporto + imposte. C_unità = C_totale / 5. Evitare doppio conteggio del display se già nella BOM. Fascia lotto aritmetica 200–350 EUR, perimetro O-19 da confermare. Prezzo finale: TBD per tutti i fornitori. Non sommare i soli componenti noti per dichiarare il budget fattibile.

## Dati aggiuntivi approvati e candidati da quotare

Service01 soltanto; buffer locale limitato e syncBLE; target standby massimo100µA a12V entro5min OFF. Quotare come opzione la coppia Molex0430200600/0430250600 a6vie con terminali corretti, pigtail interno sul singolo PCB, protezione B+ e strain relief. Non sostituire con parti a4vie senza approvazione della strategia masse. Diametro52mm resta ipotesi; fit e connettore non congelati.

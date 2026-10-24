# ERC A0 — segnalazioni aperte, non deroghe

Eseguito su gerarchia completa con KiCad10.0.7. Risultato:38segnalazioni,36errori e2warning. Nessuna esclusione creata per mascherare queste segnalazioni; il JSON include anche le regole che KiCad lascia disattivate per impostazione predefinita.

| Tipo | Numero | Posizione | Significato e chiusura |
|---|---:|---|---|
| pin_not_connected |31| J501, foglio05 | FPC interamente non assegnato. Non usare NC per dichiarare inutilizzati pin dal significato ancora sconosciuto. Occorre datasheet/codice corretto, pinout, alimentazioni, livelli e sequenze. |
| power_pin_not_driven |1| U201.22 VS | Sorgente a valle del MOSFET. ERC non deduce l’alimentazione attraversoFET; la catena di protezione ha inoltre valoriMPN/limiti aperti. Verificare circuito e poi dichiarare correttamente i power driver. |
| power_pin_not_driven |1| U202.2 VIN | DominioAON_PROTECTED a valle della protezione. Non risolto inserendo unPWR_FLAG prima di validare il dominio/wake. |
| power_pin_not_driven |1| U301.2 3V3 | Uscita regolatore attraverso induttore; mancano ancora selezione passivi e validazione del dominio. La connettività è verificata, l’alimentazione reale no. |
| power_pin_not_driven |1| U401.3 VCC | Rail5V_OBD a valle del buck/induttore; non deve essere confuso con5V_SERVICE daUSB. Completare il buck e la validazione dei domini. |
| power_pin_not_driven |1| U701.6 VIN2 | **Funzione USBOVP/limite/inrush assente.** VBUSraw eUSB_5V_PROTECTED_TBD sono deliberatamente separati; non è un semplice avviso formale. |
| isolated_pin_label |2| USB_PRESENT_SAFE_TBD, OBD_PRESENT_SAFE_TBD | IngressiMCU riservati a sensing sicuro, circuiti non definiti. Non applicare B+/VBUSdiretti e non considerare gli ingressi validi nel firmware. |

I problemi iniziali di griglia del generatore sono stati corretti. Non risultano altre classi di errore nell’export corrente; questo non prova l’assenza di errori progettuali non rilevabili daERC.

Nessun componente con valoreTBD è approvato per acquisto o popolamento. Le sole dichiarazioniPWR_FLAG presenti riguardano la sorgente esternaOBD e il suo ritorno, non le funzioni incomplete. La revisione indipendente resta da svolgere.

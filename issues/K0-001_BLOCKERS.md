# K0-001 — STOP / gate K0 non superato

Revisione 0.4, data 2026-10-24. Stato: APERTA. Priorità P0. Origine: stop condition di CODEX_KICAD_TASK_V0.2.md e SPEC §10, §11, §13.

1. **Fit pannello — aggiornato da DEC-002:** il proprietario autorizza un piccolo aumento oltre Ø44,45 mm. Il rifiuto al vecchio limite non è più causa di esclusione del candidato. Ø52 mm è ipotesi di studio; massimo finale, tolleranze e fit completo restano aperti. MECHANICAL_K0.md riporta il calcolo preliminare, non una verifica CAD.
2. **Documentazione pannello:** discrepanze interne di rail e temperatura e dati massimi di consumo insufficienti. Richiedere datasheet corretto approvato, sequenze/inizializzazione e accoppiamento FPC. Nessun footprint congelabile.
3. **Sourcing:** ricerca estesa in PANEL_SOURCING_K0.md. Huaxin MOQ1000; VIEWE prezzo pubblico ma codici PDF discordanti; LX senza stock. Nessuna conferma lotto5 con datasheet corretto. O-16 aperta.
4. **IGN/wake:** nessuna prova su Polo o simulatore; O-18 e O-06 aperte. Nessuna equivalenza automatica CAN attivo = quadro ON; nessun polling di wake in standby.
5. **Standby:** target100µA/300s approvato DEC-005; mancano metodo completo e misure dell’intero assieme all’ingresso OBD.
6. **Protezione automotive:** O-07 senza profili/limiti approvati; TVS, fusibile, MOSFET e coordinamento elettrico non dimensionati. Nessuna prova eseguita.
7. **Assemblaggio/costo:** O-17 senza conferma scritta per pannello+cavo+PCB e senza preventivo completo. Budget non verificato.
8. **Scope/storage:** Service01 approvato DEC-003; buffering locale limitato approvato DEC-004. Restano capacità/frequenza/retention/perdita e sizing memoria/OTA, non la scelta del meccanismo.

Effetto: fermare avanzamento a schematico finale, layout, export produttivi e ordini. Sono ammessi documentazione K0 e raccolta evidenze; i dati mancanti restano espliciti. Due disegni di assiemi dichiarati conformi richiedono parti documentate e verifica del fit; il candidato è riammesso allo studio, non selezionato.

Chiusura: prove e decisioni tracciate per ogni punto, disegno quotato dell'assieme reale e approvazione esplicita del gate K0 da parte del proprietario. Nessun punto è chiuso dal solo ERC/DRC.

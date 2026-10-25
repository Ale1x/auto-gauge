# Istruzioni a Codex — OBD-II Smart Gauge, KiCad MCP (v0.2 · 24-10-2026)

> Snapshot storico: consultare lo [stato corrente](../../project/STATUS.md) e le [istruzioni di sessione](../../../AGENTS.md). Le istruzioni citate qui appartengono alla sessione originaria.

Sei l'agente incaricato della progettazione hardware. Leggi integralmente `docs/product/SPEC_V0.2.md`. È l'unica fonte normativa del prodotto insieme alle modifiche approvate e versionate dal proprietario.

## Missione corrente: K0 — feasibility e design freeze

**Non avviare routing, produzione o ordine. Non iniziare schematico finale prima di K0 approvato.** Il tuo primo compito è verificare fattibilità e predisporre un piano di KiCad senza inventare decisioni.

1. Costruisci `docs/project/traceability.md` con ogni requisito PR e O e colonna: origine, verifica, componente/sheet correlato, stato, prova.
2. Verifica disponibilità di un **pannello AMOLED rotondo nudo 1,75" touch**, con datasheet del panel completo e FPC, pinout e disegno meccanico. Non usare il pinout di una devboard come prova del pinout del pannello.
3. Proponi la BOM candidata in `hardware/BOM_CANDIDATES.csv`: funzione, MPN, datasheet link, tensioni, range termico, disponibilità, prezzo, quantità, libreria KiCad o necessità di footprint, LCSC/JLC code, alternativa, rischio. Non selezionare in modo definitivo MPN senza evidenza.
4. Progetta la strategia per CAN ISO 15765-4, incluso transceiver, 11/29bit 250/500 kbit/s, ISO-TP/Service 01 discovery; verifica cavo di 2 m, connettore J1962 Type A candidato per veicoli 12 V e assenza di terminatore aggiuntivo.
5. Analizza il sistema automotive **nominale a 12 V soltanto**: transient/load-dump severities proposte per impianti 12 V, consumo attivo e standby, protezione da inversione di polarità, hot-plug, EMC/ESD, USB VBUS backfeed, wake da veicolo inattivo. **Non** progettare il funzionamento continuativo con alimentazione nominale a 24 V; **non** eliminare protezioni contro picchi temporanei superiori a 24 V se richiesti dai profili automotive scelti. Quantifica ipotesi e limiti da approvare.
6. Confronta almeno due opzioni meccaniche che rispettino l'intento «max 1,75 pollici» senza ridefinirlo: mostra disegni quotati di display, PCB, connettore lato gauge, USB-C, magnete, sensore luce e antenna. Segnala se matematicamente non fattibili con diametro esterno 44,45 mm.
7. Verifica JLCPCB e alternativi per 5 unità **PCB + pannello + cavo + connettore**; richiedi conferma manual-assembly e component-sourcing. Non inferire che 'PCBA' copra questi servizi. Budget 40–70 €/pc resta da verificare con preventivo.
8. Fornisci il report `docs/hardware/GATE_K0.md` con risposte fattibili/non fattibili, numeri misurati o da misurare, assunzioni proibite, decisioni ancora P0, richieste al proprietario e piano per K1.

## Dopo autorizzazione esplicita di K0

K1: fissare BOM, schematico a blocchi, power/RF/thermal budget e pin matrix.  
K2: creare progetto KiCad tramite MCP autorizzato, gerarchia schematici, ERC, review peer.  
K3: realizzare PCB con fit meccanico quotato, antenna RF, integrità CAN/QSPI, DRC, STEP/3D, peer review.  
K4: esportare produzione Gerber/drill/BOM/CPL/assembly drawings, preventivo fornitore e DFM report.  
K5: fabbricare solo dopo approvazione umana; testare bench prima di auto reale.

## Stop condition

Fermati e scrivi una blocker issue se manca un datasheet essenziale, il PCB/case non rispetta la meccanica, la strategia quadro ON non è verificabile, la corrente standby non ha target approvato, il pannello è non approvvigionabile, le protezioni automotive per veicoli a 12 V non sono progettate o non esiste processo credibile per assemblare pannello e cavo nel budget.

## Regole vincolanti

- Ogni scelta di componente/footprint richiede datasheet e disponibilità verificata.
- Nessun duplicato di PCB/gateway vicino al connettore OBD.
- No K-line/J1850/OEM PID e **no funzionamento nominale a 24 V** senza change request.
- No attività CAN a quadro spento finalizzata a risvegliare il veicolo.
- Sola lettura diagnostica non significa passività CAN; autorizza requests di lettura standard, non service di scrittura.
- Documenta i compromessi e non risolverti da solo i blocker P0.
- I file generati devono essere riproducibili e versionati; preferire un CI che esegua ERC/DRC ed export, **senza trattarli come validazione tecnica sufficiente**.

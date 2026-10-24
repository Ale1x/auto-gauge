# Cavo e potenza — inviluppo analitico K0, 2026-10-24

Generato da scripts/build_power_envelope.py. Nessuna misura, sezione cavo scelta o garanzia di regolazione. Carichi, efficienze e temperature sono ipotesi di sensibilità, non nuovi requisiti.

## Modello

Cavo lungo2m, un conduttore B+ e un ritorno di potenza: lunghezza elettrica totale4m. Nessun parallelo implicito delle masse OBD4/5. R_cavo =4 ×0,0175/A ×[1+0,00393(T−20)] Ω, con A inmm². Resistività e coefficiente termico sono ipotesi numeriche del modello, da sostituire con resistenza massima/km e tolleranze del cavo esatto. T è temperatura del conduttore, non ambiente né qualificazione a85°C.

Aggiunti0,10Ω complessivi H per contatti/fusibile/giunzioni: non sono massimi da datasheet. Non includono perdite a corrente costante dei controller né caduta TVS dinamica. Potenza d’ingresso al gauge =P_carico/η; soluzione del ramo a corrente minore di I(V_sorgente−I R_tot)=P_carico/η. La soluzione statica non prova stabilità del convertitore, avvio o risposta ai transitori.

## Esempi con sezione0,22mm², conduttore85°C ed efficienza85%

| Sorgente V | Carico W | Corrente A | Ingresso gauge V | Perdita cavo W | Perdita conversione W |
|---:|---:|---:|---:|---:|---:|
| 6 | 2 | 0.406 | 5.797 | 0.066 | 0.353 |
| 6 | 4 | 0.844 | 5.579 | 0.284 | 0.706 |
| 9 | 2 | 0.265 | 8.867 | 0.028 | 0.353 |
| 9 | 4 | 0.539 | 8.731 | 0.116 | 0.706 |
| 12 | 2 | 0.198 | 11.901 | 0.016 | 0.353 |
| 12 | 4 | 0.399 | 11.801 | 0.064 | 0.706 |
| 14.4 | 2 | 0.164 | 14.318 | 0.011 | 0.353 |
| 14.4 | 4 | 0.331 | 14.235 | 0.044 | 0.706 |

A6V alla presa, la tensione al gauge scende sotto6V con ogni carico non nullo e resistenza positiva. Il buck candidato LM5164-Q1 ha ingresso minimo dichiarato6V (E04): lo screening crank6V alla presa non può essere descritto come funzionamento regolato garantito al gauge. DEC-008 ammette reset durante crank con ripartenza automatica; non serve garantire continuità del display. Restano da fissare finestra UV/OV, profilo e latenza di recupero, e da verificare integrità log/config/OTA al brownout. A6V d’ingresso al buck, il margine reale per5V può richiedere ulteriori verifiche di dropout e carico.

Un buck5V/1A offre nominalmente5W all’uscita prima dei limiti termici. Se i carichi finali richiedono4W e un successivo stadio ha efficienza85%, servono circa4,706W e0,941A sul rail5V, prima dei carichi5V diretti e perdite ausiliarie: margine nominale insufficiente per una scelta definitiva. Non confondere corrente OBD con corrente del rail5V.

La dissipazione del convertitore è nel case; quella del cavo è distribuita fuori dal case. L’ipotesi0,10Ω aggrega anche elementi interni: per il thermal budget reale separare ogni componente, posizione e curva di efficienza. Nessuna temperatura interna prevista senza resistenze termiche/geometria e misure.

## Fusibile, inrush e cortocircuito

La caduta statica non determina ampacità né fusibile. Prima del freeze ottenere corrente massima continua/di picco, sezione e isolamento reali, derating in fascio, resistenza contatti a fine vita, capacità d’ingresso e corrente di spunto. Coordinare curva tempo-corrente e I²t del fusibile con protezione del cavo e SOA MOSFET, compresi corto remoto e corto vicino alla presa; verificare capacità d’interruzione DC alla corrente di guasto disponibile. Nessun valore fusibile derivato dal solo esempio2W.

## Blocco specifico del wake

E05 §8.4.2: LM74900-Q1 in SLEEP usa un percorso interno da7Ω tipici e supporta picchi100mA; sovracorrente provoca latch-off, con soglia tipica250mA (non corrente operativa permessa). Occorre uscire da SLEEP prima dell’avvio del carico principale. Non alimentare MCU/display attraverso il bypass assumendo che il MCU possa poi abilitare il proprio percorso.

Ipotesi architetturale da verificare: evento INH del transceiver alimentato nel dominio always-on → controllo compatibile con livelli ad alta tensione → uscita SLEEP e abilitazione del percorso MOSFET → buck/carico con ritardo o power-good appropriato. Solo dopo stabilità rail, MCU in ascolto e qualificazione dello stato. INH è segnale logico ad alta tensione, non alimentatore (E03 §7.3.4); evitare clamp parassiti e collegamento diretto a3,3V. Ritardi, default dei controlli e correnti di spunto restano da progettare.

Prove di chiusura: cold start, wake CAN, USB sola, OBD+USB, interruzione mentre SLEEP cambia, batteria bassa, fault/OV, latch-off e recupero senza dover scollegare il gauge. Lo schema del dominio always-on deve mantenere protezione da inversione/transienti: nessun bypass diretto da B+ non protetto. Il wake CAN non risolve da solo IGN.

## Esito K0

288 casi parametrici in evidence/models/power_cable_sensitivity.csv. Nessuna riga è un PASS hardware. O-05/O-07/O-18 e scelta sezione/fusibile restano aperti; i risultati restringono le configurazioni che si possono dichiarare plausibili. Fonti E03/E04/E05, insieme alle ipotesi esplicite sopra.

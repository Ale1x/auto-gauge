# Renderer RGB565 incrementale — V0.13

`ag_gauge_renderer` converte la scena reale `ag_gauge_ui` in righe RGB565 con
font ASCII5×7 implementato localmente. Non dipende da framebuffer heap, librerie
grafiche esterne, GPIO, controller CO5300, protocollo QSPI, touch o DMA. La
configurazione usa coordinate normalizzate0…10000, come il validator del core.

## Contratto del sink

Il proprietario serializzato inizializza `ag_gauge_renderer_options_t` con
larghezza/altezza32…466 e `write_row(context,y,pixels,width)`. Il callback riceve
una riga completa di parole RGB565 CPU e deve copiarle/consumarle prima di
restituire successo: il buffer di466pixel (932byte) viene riusato alla chiamata
seguente. L'adapter decide endianess bus, finestra/offset pannello, comandi,
frequenza, sincronizzazione e politica elettrica. Nessun pin o livello è dedotto
qui dai file del fornitore.

`begin` valida e copia tutta la scena prima di qualunque IO; rifiuta dimensioni,
conteggi, ID, enum e rettangoli invalidi. Una scena già in corso non è sostituita
silenziosamente; `BUSY` lascia lo snapshot corrente intatto. `step` emette al
massimo1…16righe per chiamata. Il chiamante sceglie budget e frequenza in base
alle risorse misurate del driver reale. Nessuna frequenza di aggiornamento o
latenza fisica è qualificata dalle prove host.

Ogni riga riscrive lo sfondo, quindi valori/widget precedenti vengono rimossi
senza un framebuffer completo. I pixel esterni al cerchio inscritto sono neri,
anche quando il rettangolo widget supera il bordo curvo. Il layout mostra
`PID XX`, valore con una cifra decimale e unità. Un valore è visibile soltanto
con powerON, fonte esplicita, has_value, qualityVALID e numero finito. Zero resta
`0.0`; qualità stale/errore/unavailable o assenza fonte diventano `--` e unità`?`.
Valori oltre±999999999 diventano `RANGE`, senza stringhe troncate. Le aree troppo
piccole per un glifo alla scala minima non mostrano testo tagliato. Il font e
l'impaginazione sono una prima implementazione software, non un design prodotto
approvato o una traduzione fisicamente misurata del pannello.

PowerOFF produce nero; powerUNAVAILABLE una diagnostica `POWER ?` senza misure.
La percentuale di luminosità non viene convertita in gamma/PWM/comandi: quel
controllo appartiene al boardprofile e alla policy ALS/power.

## Fault, cancellazione e integrazione

Un callback sink fallito può avere già scritto parte della riga: il renderer
latcha FAULT, interrompe lo snapshot e non ripete automaticamente IO. Le API non
cancellano tale evidenza. L'owner deve decidere la recovery dopo aver reso
quiescente l'adapter reale.

`cancel_pending` scarta soltanto lo snapshot RAM non ancora completato e non
esegue IO. Serve per sottomettere una scena sicura durante fault/power/config
change senza continuare a generare vecchie righe. Le righe già emesse rimangono
sul pannello finché non vengono riscritte o oscurate dall'adapter fisico: il
renderer non dichiara uno spegnimento immediato. Il callback non può rientrare
nel renderer; BUSY preserva lo stato durante `write_row`.

Il hook `render` dell'applicazione può fare `begin` quando il renderer non è
attivo, poi `step(...,16)` per tick; se arriva una scena sicura deve cancellare
la vecchia scena in corso prima di sottometterla. L'owner applicativo può
continuare discovery/runtime/log/RPC fra budget di righe. Il profilo software
banco può consumare le righe in un sink diagnostico senza attivare un display.

## Verifica

`firmware/tests/test_gauge_renderer.c`, quattro gruppi con ASan/UBSan e warnings
come errori:

- snapshot immutabile, budget massimo, zero e negativo, pixel/font/unità e clip;
- tutte le qualità non valide, fonte assente, NaN/infinito, powerOFF/unavailable;
- input invalidi senza IO, rientranza, errore sink con latch, cancellazione scena;
- range numerico enorme, rettangolo minimo, full-frame e margini circolari.

Il test può esportare una PPM466×466 impostando `AG_RENDER_PREVIEW` al percorso
di uscita; il file proviene dai pixel generati dal vero renderer, non da un
mockup indipendente. La preview con2000RPM e−12,5C è stata convertita in PNG e
ispezionata. Queste prove non qualificano leggibilità in auto, timing QSPI,
consumo, pannello, antialiasing, touch o interazione fisica.

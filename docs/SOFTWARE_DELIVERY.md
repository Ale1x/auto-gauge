# Consegna software riproducibile

Il comando seguente verifica e raccoglie una build locale, senza accesso a schede,
flash, eFuse, firma iOS, cloud, upload o installazione di dipendenze:

```sh
python3 scripts/build_software_bundle.py
```

Richiede gli strumenti di sviluppo già installati e la cache ESP-IDF fissata dal
progetto. L'output predefinito è `output/software/`, escluso da Git. La directory
deve essere vuota: per conservare una consegna precedente usare un nuovo percorso
con `--output /percorso/nuova-consegna`. Nessuna directory sorgente viene pulita.

Il comando esegue i check Python/C/Swift condivisi, compila il firmware default
e quello banco, il target nativo con adapter SDK unificati e profilo locale
assente, i tre studi SDK BLE/OTA/platform e le app iOS simulatore/device
unsigned. Conserva log separati, ELF e immagini con le rispettive configurazioni
e i due prodotti `.app`. `manifest.json` contiene SHA-256 di ciascun artefatto e
dei sorgenti software presenti (anche quelli non ancora committati), commit base,
stato del working tree, profili e confini delle verifiche. Gli originali fornitori
in `email-response/` non entrano nella consegna né nel manifest software.

I nomi dei binari sono ricavati dal progetto effettivamente compilato: eventuali
vecchie immagini rimaste nella directory build non vengono incluse. Un errore
ferma il processo e lascia i log della consegna parziale; il manifest finale viene
scritto soltanto dopo che tutte le fasi hanno avuto successo. Il comando non
stabilisce da solo la compatibilità con le due schede non identificate.

Il firmware default resta bloccato fino a un board profile locale qualificato.
Il profilo banco usa CAN/storage/display simulati dichiarati; gli studi SDK hanno
main inerti. Le app unsigned non sono una distribuzione App Store o un installer
per iPhone. La consegna permette di riprodurre il software prima delle prove
fisiche, non sostituisce queste ultime né la qualifica CloudKit.

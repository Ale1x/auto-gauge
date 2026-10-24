# Ammissione di nuovi telefoni — flusso approvato in DEC-014

24 ottobre 2026. Il proprietario ha approvato il flusso UX in DEC-014; **implementazione e parametri tecnici restano da verificare**. DEC-011 è già fissata: più telefoni sequenziali, una connessione attiva, nessun owner unico e nessun reset per cambiare telefono. Il cloud non concede o revoca l’uso del gauge.

## Flusso approvato

1. Per un telefono nuovo, la persona apre sul gauge una funzione locale «Associa telefono». L’interazione deve essere disponibile nel menu touch; gesto e timeout sono da dimensionare e verificare, non requisiti congelati.
2. Il gauge mostra un codice casuale a sei cifre per il tentativo corrente. La persona lo inserisce nella richiesta Bluetooth di iOS. Nessun codice fisso, pubblicato negli advertising o scritto nei log.
3. Si richiedono LE Secure Connections e pairing autenticato, senza fallback Just Works/legacy. Solo dopo conferma dello stato crittografico effettivo sono ammesse letture di dati privati e mutazioni. CRC, connessione o request ID non sono prove di autenticazione.
4. I telefoni già riconosciuti possono riconnettersi quando lo slot è libero. La seconda connessione non sostituisce quella attiva. La riconnessione deve ripristinare il link sicuro prima di riprendere i comandi.
5. La memoria dei bond è una cache tecnica finita, **non un N massimo di telefoni consentiti**. Se occorre liberare una voce non attiva, quel telefono potrà ripetere il normale abbinamento senza reset o cancellazione configurazione/log. Numero, politica e UX della cache restano da progettare; non eliminare automaticamente un bond alla richiesta remota di repeat-pairing.
6. Reset fisico USB/pulsanti resta la manutenzione distinta già approvata. Non è necessario per autorizzare B dopo A e non cancella il cloud.

L’alternativa prototipo è richiedere il codice locale a ogni collegamento, senza affidarsi a riconoscimento persistente: riduce la gestione bond ma aggiunge un’azione a ogni uso. Il proprietario ha scelto la riconnessione dei telefoni riconosciuti; l’alternativa a ogni collegamento non è stata scelta.

## Vincoli tecnici prima del runtime reale

- Verificare API e flag sul tag ESP-IDF v6.1 fissato, impostando un solo link e controllando encryption/authentication/SC/key strength sul descriptor reale. Le impostazioni del progetto non bastano come prova della negoziazione.
- Proteggere anche notification/indication: il flag di accesso cifrato su read/write non dimostra da solo che l’invio asincrono sia autorizzato. Nessun payload privato prima della verifica.
- Timeout per sessione non autenticata e tentativi limitati, cooldown e chiusura pairing window; misure bounded e guardia a ogni mutazione, senza nuovi vincoli di ownership.
- Serializzare eventi; invalidare lease e messaggi pendenti su disconnect/reset. Non associare MAC Bluetooth a identità stabile senza privacy/identity resolution del protocollo.
- Il pairing deve funzionare anche senza iCloud/membership Apple. L’app non legge le chiavi Bluetooth né implementa crittografia custom al posto dello stack.
- Non confondere link sicuro con OTA firmata: verifica immagine/compatibilità/rollback e gestione chiavi restano separati.
- Test con due telefoni A→B→A, perdita bond, storage pieno, repeat-pairing ostile, timeout, downgrade e callback tardivi; nessun TX CAN autorizzato dal pairing o dalla presenza USB.

## Fonti primarie consultate

[Espressif SMP v6.1](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-guides/ble/smp.html) distingue pairing, cifratura e bonding. [Kconfig ESP32-S3 v6.1](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/kconfig-reference.html) espone SC-only, security level, persistenza e numero di bond: default non equivale a configurazione sicura. [Esempio ufficiale NimBLE Security](https://github.com/espressif/esp-idf/tree/v6.1/examples/bluetooth/ble_get_started/nimble/NimBLE_Security) illustra passkey display e avvio cifratura, ma non è una policy completa del prodotto; non copiare logging passkey, modulo casuale o cancellazione automatica dei peer come decisioni definitive.

DEC-014 risolve la scelta dell’esperienza di ammissione; non va riproposta la stessa domanda. Non attiva Bluetooth, non crea chiavi, non cambia DEC-011 e non dichiara una review di sicurezza completa.

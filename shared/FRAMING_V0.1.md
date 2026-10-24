# Framing di sviluppo v0.1

24/10/2026. Ipotesi di implementazione per simulatori e test, compatibile con il modello di configurazione PROTOCOL_V0.1.md. Non abilita GATT, autenticazione o operazioni reali sul gauge.

## Packet

| Offset | Byte | Significato |
|---:|---:|---|
| 0 | 2 | ASCII `AG` |
| 2 | 1 | Versione framing, 1 |
| 3 | 1 | Kind: 1 request, 2 response, 3 event |
| 4 | 4 | Request ID unsigned little endian; nonzero per request/response, zero per event |
| 8 | 2 | Lunghezza payload unsigned little endian, 0–4096 |
| 10 | 2 | Riservati, obbligatoriamente zero |
| 12 | N | Payload; i messaggi applicativi di sviluppo usano JSON UTF-8 |
| 12+N | 4 | CRC32 IEEE su header e payload, little endian; polinomio riflesso EDB88320, init/final XOR FFFFFFFF |

Overhead16 byte, packet massimo4112. Nessun padding. Header/versioni/tipi/riservati/lunghezza/CRC non validi fanno fallire il messaggio. CRC rileva corruzione accidentale: **non autentica, non cifra e non impedisce replay**. Un request ID è correlazione, non una prova di freschezza o proprietà.

## Frammentazione

Il decoder stream di riferimento accetta chunk fino a512 byte, ma il futuro adapter BLE deve usare il limite effettivo della singola connessione e operazione, potenzialmente molto inferiore. Nessun packet da4112 viene scritto come un singolo valore ATT. Non è previsto un MTU fisso. Il valore512 è un limite superiore di sviluppo del decoder, non un payload garantito da BLE.

Un solo frame incompleto per connessione, memoria limitata a4112 byte. Header verificato appena completo; timeout1000ms dall'inizio del frame è un'ipotesi di sviluppo. Timeout, errore o disconnessione svuotano lo stato; il mittente riparte da un frame intero. Il timeout non si prolunga a ogni frammento. La scadenza va valutata anche quando non arrivano dati. Nessun resync cercando magic dentro payload corrotto. Stream TCP/UART ed effettivo adattatore BLE non sono implementati.

I vettori golden in `fixtures/frame.cases.json` sono condivisi fra Python, C e l'eventuale codec Swift. Il codec C tratta packet completi con vista presa in prestito sul payload; reassembly e timer spettano all'adapter. Non passare strutture C interne del firmware come byte wire.

## Sicurezza e dipendenze

Le operazioni che mutano il dispositivo restano bloccate nel trasporto reale. Il simulatore può applicare in RAM una configurazione indicando esplicitamente `simulation=true` e `persistence=volatile`; non è conferma di salvataggio sul gauge. DEC-011 consente più client sequenziali e una sola connessione attiva, senza ownership esclusiva o reset per cambiare telefono. Reset locale e protocollo autenticato richiedono ancora implementazione e prove. Non scegliere chiavi, eFuse o provider sulla base di questo framing.

Fonti primarie consultate il24/10/2026: [Bluetooth ATT](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/host/attribute-protocol--att-.html), [Apple scritture CoreBluetooth](https://developer.apple.com/documentation/corebluetooth/cbcharacteristicwritetype). Limiti di notifica e write dipendono da ATT e dalle API; prima dell'implementazione reale riverificare la versione normativa applicabile e `maximumWriteValueLength`.

## Sessione OTA dedicata

La [sessione OTA framed](OTA_FRAMED_V0.1.md) usa gli stessi kind1/2 e codec, con payload OTA binari chiusi al posto del JSON e ID consecutivi. Nessuna modifica ai vettori framing o al parser RPC; il contenuto del packet non attiva da solo la route OTA. Lo studio SDK collega l’endpoint dedicato e il trasporto Swift è testato su pipe host, con gate di sviluppo chiuso per default. Nessuna radio viene avviata da questa integrazione.

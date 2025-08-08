# ESP32 OBD BLE — Dashboard + Simulator

Sistema end-to-end per inviare dati OBD via **USB → ESP32 → BLE** e visualizzarli **in tempo reale** su una web-app (Web Bluetooth).  
Include anche: salvataggio degli errori su **SPIFFS**, download/reset degli errori da browser, e un **simulatore** di telemetria auto.

---

## Architettura del sistema

- **Modulo Python**: genera in maniera simulata (o acquisisce da un’interfaccia OBD reale) pacchetti dati nel formato `chiave:valore;...` e li trasmette tramite interfaccia **seriale** a 115200 baud verso l’ESP32.  
- **Firmware ESP32**: funge da bridge **UART ↔ BLE** con notifiche in tempo reale, esegue il **parsing** dei pacchetti ricevuti e, in presenza di codici di errore, effettua il **salvataggio (“snapshot”)** su memoria SPIFFS in formato CSV.  
- **Interfaccia Web**: applicazione **HTML/JavaScript** che, tramite **Web Bluetooth API**, si connette al servizio BLE (UUID `0xFF00`, caratteristica `0xFF01`) dell’ESP32 per la visualizzazione in tempo reale dei dati telemetrici, la consultazione dei registri di errore e l’esecuzione di comandi di gestione (`GET_ALL_ERRORS`, `RESET_ERRORS`).
---

## Repository

```
├─ firmware/
│  ├─ main.c                # bridge UART↔BLE + parser + SPIFFS init
│  ├─ ble_server.c          # GATT service FF00 / char FF01 + comandi
│  ├─ error_snapshot.{c,h}  # snapshot errori in RAM + /spiffs/snapshots.txt
├─ simulator/
│  ├─ randomdata.py         # genera dati auto realisti via seriale
│  └─ obd-reader.py         # (opz.) lettura reale con python-OBD
└─ web/
   ├─ index.html            # dashboard Web Bluetooth
   └─ style.css
```

---

## Requisiti

- **ESP-IDF** (v4.x+ consigliato) e toolchain per ESP32.
- **Python 3.8+** con `pyserial` (simulatore) e, per l’uso reale, `obd` (python-OBD).
- Un **ESP32** con USB-serial (UART0 a 115200).
- Browser **Chromium-based** con Web Bluetooth (ok **localhost** o **HTTPS**).

Install pacchetti Python (simulatore + OBD reale):

```bash
python -m pip install pyserial obd
```

---

## Quickstart

### 1) Compila e flasha il firmware ESP32

```bash
idf.py set-target esp32
idf.py menuconfig
idf.py -p /dev/ttyUSB0 flash monitor
```

Il firmware:
- inizializza **SPIFFS** su `/spiffs` (auto-format se serve),  
- avvia **BLE** con device name `ESP32_OBD_BLE`,  
- crea **GATT**: **Service 0xFF00** e **Characteristic 0xFF01** con **notify/write**.

### 2) Avvia il **simulatore**

```bash
python simulator/randomdata.py
```

Il pacchetto spedito ha forma:

```
VEICOLO:...;MARCA:...;MODELLO:...;ANNO:...;VIN:...;TARGA:...;KM:...;
RPM:...;SPEED:...;TEMP:...;FUEL:...;MIL:...;ERROR_CODES:...;
```

### 3) Avvia la **web-app**

```bash
cd web
python -m http.server 8000
```

Apri: [http://localhost:8000](http://localhost:8000) → **Connetti BLE**.

---

## Protocollo BLE

- **Service** `0xFF00`, **Characteristic** `0xFF01` (**READ/WRITE/NOTIFY**).
- **Comandi**:
  - `GET_ALL_ERRORS` → lista CSV errori + `END_OF_ERRORS`
  - `RESET_ERRORS` → cancella `/spiffs/snapshots.txt` + `ERRORS_CLEARED`

---

## Snapshots errori

- Salvataggio in RAM (max 10) **e** su `/spiffs/snapshots.txt`.
- CSV: `timestamp_us,error_code,rpm,speed,temp,fuel,mil,marca,modello,vin,targa,km`.

---

## Esecuzione demo

1. Flash firmware ESP32.  
2. Avvia `randomdata.py`.  
3. Apri web-app e connetti BLE.

---

## Troubleshooting

- **Nessun device**: assicurati advertising attivo e browser compatibile.
- **No dati**: controlla baud/porta corretti.
- **SPIFFS vuoto**: verifica partizione in partition table.

---

# ESP32 OBD BLE — Dashboard + Simulator

Sistema end-to-end per inviare dati OBD via **USB → ESP32 → BLE** e visualizzarli **in tempo reale** su una web-app (Web Bluetooth).  
Include anche: salvataggio degli errori su **SPIFFS**, download/reset degli errori da browser, e un **simulatore** di telemetria auto.

---

## Architettura in 1 minuto

- **Python** genera pacchetti `key:value;...` (o legge davvero dall’OBD) e li spara sulla **seriale** a 115200.  
- **ESP32 firmware** fa da bridge **UART↔BLE notify**, effettua il **parsing** e, se rileva errori, **snapshotta** su SPIFFS (CSV).  
- **Web app** (HTML/JS) si connette via **Web Bluetooth** (service `0xFF00`, char `0xFF01`) e mostra dashboard, log e gestione errori (GET/RESET).

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

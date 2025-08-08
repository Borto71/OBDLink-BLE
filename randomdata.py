import random
import time
import serial

# === Configura qui le tue auto ===
VEICOLI = [
    {
        "nome": "Fiat Punto",
        "marca": "Fiat",
        "modello": "Punto 1.2 8v",
        "anno": 2006,
        "vin": "ZFA19900001234567",
        "targa": "AB123CD",
        "km": 172340,
        "rpm_min": 800,
        "rpm_max": 6200,
        "temp_min": 75,
        "temp_max": 105,
        "fuel_min": 13,
        "fuel_max": 95,
        "errori_possibili": [
            [], ["P0420"], ["P0301"], ["P0300"], []
        ]
    },
    {
        "nome": "Volkswagen Golf",
        "marca": "VW",
        "modello": "Golf VII 1.6 TDI",
        "anno": 2017,
        "vin": "WVWZZZAUZHP123456",
        "targa": "FG567HI",
        "km": 102300,
        "rpm_min": 900,
        "rpm_max": 5400,
        "temp_min": 70,
        "temp_max": 100,
        "fuel_min": 24,
        "fuel_max": 90,
        "errori_possibili": [
            [], ["P2002"], ["P2452"], ["P0401"], []
        ]
    },
    {
        "nome": "Tesla Model 3",
        "marca": "Tesla",
        "modello": "Model 3 LR AWD",
        "anno": 2022,
        "vin": "5YJ3E1EA7LF123456",
        "targa": "EL123EV",
        "km": 25100,
        "rpm_min": 0,
        "rpm_max": 18000,
        "temp_min": 20,
        "temp_max": 75,
        "fuel_min": 25,   # in % batteria!
        "fuel_max": 98,
        "errori_possibili": [
            [], ["P1A10"], ["P1A09"], [], ["BMS1234"]
        ]
    },    
    {
        "nome": "BMW Serie 3",
        "marca": "BMW",
        "modello": "Serie 3 320d",
        "anno": 2017,
        "vin": "WBA8A9C53H123456",
        "targa": "XY987ZK",
        "km": 123456,
        "rpm_min": 0,
        "rpm_max": 6000,
        "temp_min": 20,
        "temp_max": 75,
        "fuel_min": 25,   
        "fuel_max": 98,
        "errori_possibili": [
            [], ["P1A10"], ["P1A09"], [], ["BMS1234"]
        ]
    }
]

# === Imposta la seriale (modifica se serve) ===
SERIAL_PORT = '/dev/ttyUSB0'     # Cambia a COMx su Windows!
BAUDRATE = 115200
INTERVALLO = 1.5                 # Secondi tra un pacchetto e l'altro

def genera_dati_obd(veicolo):
    dati = {
        "VEICOLO": veicolo["nome"],
        "MARCA": veicolo["marca"],
        "MODELLO": veicolo["modello"],
        "ANNO": veicolo["anno"],
        "VIN": veicolo["vin"],
        "TARGA": veicolo["targa"],
        "KM": veicolo["km"] + random.randint(0, 8),  # simula avanzamento km
        "RPM": random.randint(veicolo["rpm_min"], veicolo["rpm_max"]),
        "SPEED": random.randint(0, 135),
        "TEMP": random.randint(veicolo["temp_min"], veicolo["temp_max"]),
        "FUEL": round(random.uniform(veicolo["fuel_min"], veicolo["fuel_max"]), 1),
        "MIL": random.choice([0, 1]),
        "ERROR_CODES": ",".join(random.choice(veicolo["errori_possibili"])) if random.random()<0.25 else "NONE"
    }
    campi = [f"{k}:{v}" for k, v in dati.items()]
    pacchetto = ";".join(campi) + ";\n"
    return pacchetto

if __name__ == "__main__":
    print("=== Random OBD BLE Simulator (multi-veicolo) ===\n")
    for i, veicolo in enumerate(VEICOLI):
        print(f"{i+1}) {veicolo['nome']} ({veicolo['targa']}, {veicolo['km']} km)")

    try:
        idx = int(input(f"\nScegli veicolo [1-{len(VEICOLI)}] (default 1): ")) - 1
    except Exception:
        idx = 0

    if idx < 0 or idx >= len(VEICOLI):
        idx = 0

    veicolo = VEICOLI[idx]
    print(f"\nUserai: {veicolo['nome']} ({veicolo['targa']}, VIN {veicolo['vin']})")
    print(f"Aprendo porta seriale su {SERIAL_PORT} a {BAUDRATE} baud...\n")

    try:
        serial_port = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1)
    except Exception as e:
        print(f"ERRORE: {e}")
        exit(1)

    while True:
        pacchetto = genera_dati_obd(veicolo)
        print(f"Inviato: {pacchetto.strip()}")
        serial_port.write(pacchetto.encode())
        time.sleep(INTERVALLO)

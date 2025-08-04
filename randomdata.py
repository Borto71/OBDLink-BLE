import random
import time
import serial

# Inizializza la porta seriale ESP32 (modifica se necessario)
serial_port = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)

def genera_dati_obd():
    dati = {
        "RPM": random.randint(800, 4000),
        "SPEED": random.randint(0, 130),
        "TEMP": random.randint(70, 105),
        "FUEL": round(random.uniform(10.0, 90.0), 1),
        "MIL": random.choice([0, 1]),
        "ERROR_CODES": random.choice([
            [], ["P0420"], ["P0301", "P0420"], []
        ])
    }

    # Formatta stringa tipo: RPM:1500;SPEED:50;ERROR_CODES:P0420,P0301;
    campi = []
    for k, v in dati.items():
        if isinstance(v, list):
            campi.append(f"{k}:{','.join(v) if v else 'NONE'}")
        else:
            campi.append(f"{k}:{v}")
    pacchetto = ";".join(campi) + ";\n"
    return pacchetto

if __name__ == "__main__":
    while True:
        pacchetto = genera_dati_obd()
        print(f"Inviato: {pacchetto.strip()}")
        serial_port.write(pacchetto.encode())
        time.sleep(1)

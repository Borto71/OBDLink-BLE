import random
import time
import serial

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

    campi = []
    for k, v in dati.items():
        if isinstance(v, list):
            campi.append(f"{k}:{','.join(v) if v else 'NONE'}")
        else:
            campi.append(f"{k}:{v}")
    pacchetto = ";".join(campi) + ";\n"
    return pacchetto

if __name__ == "__main__":
    print("=== Random OBD BLE Simulator ===")
    porta = input("Inserisci la porta seriale dell'ESP32 (es: /dev/ttyUSB0): ").strip()
    if not porta:
        porta = '/dev/ttyUSB0'
    try:
        serial_port = serial.Serial(porta, 115200, timeout=1)
    except Exception as e:
        print("Errore apertura porta seriale:", e)
        exit(1)

    print("Collegati prima via web app (Scan & Connect), poi premi INVIO per iniziare a inviare dati random.")
    input("Pronto? Premi INVIO per partire...")

    intervallo = input("Ogni quanti secondi vuoi inviare dati? [default: 2] ").strip()
    try:
        intervallo = float(intervallo)
        if intervallo < 0.2:
            intervallo = 0.2
    except:
        intervallo = 2.0

    print(f"Invio dati ogni {intervallo} secondi. Ctrl+C per fermare.")
    try:
        while True:
            pacchetto = genera_dati_obd()
            print(f"Inviato: {pacchetto.strip()}")
            serial_port.write(pacchetto.encode())
            time.sleep(intervallo)
    except KeyboardInterrupt:
        print("\nInterrotto da tastiera, chiudo.")
    finally:
        serial_port.close()

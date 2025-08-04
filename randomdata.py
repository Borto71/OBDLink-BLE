import random
import time
import serial

# Imposta la porta seriale giusta!
serial_port = serial.Serial('/dev/ttyUSB0', 115200)

def genera_dati_obd():
    rpm = random.randint(700, 4000)
    speed = random.randint(0, 130)
    temp = random.randint(70, 110)
    error = random.choice([0, 1])
    pacchetto = f"RPM:{rpm};SPEED:{speed};TEMP:{temp};ERROR:{error};\n"
    return pacchetto

if __name__ == "__main__":
    while True:
        pacchetto = genera_dati_obd()
        print(pacchetto, end='')  # stampa anche su console
        serial_port.write(pacchetto.encode())  # invia su seriale
        time.sleep(1)

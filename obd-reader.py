import obd
import time

# Connetti alla porta OBD-II (modifica la porta se serve, es: '/dev/ttyUSB0' o 'COM5')
connection = obd.OBD()  # tenta di aprire la prima porta disponibile

if not connection.is_connected():
    print("Errore: non è stato possibile connettersi all'adattatore OBD")
    exit(1)

# Comandi OBD-II comuni
cmds = {
    'RPM': obd.commands.RPM,
    'SPEED': obd.commands.SPEED,
    'COOLANT_TEMP': obd.commands.COOLANT_TEMP,
    'THROTTLE_POS': obd.commands.THROTTLE_POS,
    'FUEL_LEVEL': obd.commands.FUEL_LEVEL,
    'MIL_STATUS': obd.commands.STATUS_MIL,
    'DTC_COUNT': obd.commands.DTC_NUMBER,
}

def print_dtcs(connection):
    dtcs = connection.query(obd.commands.GET_DTC)
    if dtcs.is_null():
        print("Nessun codice DTC attivo")
    else:
        print("Codici DTC attivi:")
        for code, desc in dtcs.value:
            print(f" - {code}: {desc}")

try:
    while True:
        for name, cmd in cmds.items():
            response = connection.query(cmd)
            if not response.is_null():
                print(f"{name}: {response.value}")
            else:
                print(f"{name}: N/D")
        print_dtcs(connection)
        print("-" * 40)
        time.sleep(1)
except KeyboardInterrupt:
    print("Termino la lettura OBD")
    connection.close()

# Registra un componente ESP-IDF (questa cartella è un "component")
idf_component_register(
    # Sorgenti C del componente (compilati e linkati in questo target)
    SRCS "main.c" "ble_server.c" "error_snapshot.c"

    # Directory da cui esporre gli header a chi dipende dal componente
    INCLUDE_DIRS "."
)

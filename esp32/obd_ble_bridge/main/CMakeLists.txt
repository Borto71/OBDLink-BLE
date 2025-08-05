idf_component_register(
    SRCS "ble_server.c" "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES bt driver nvs_flash
)





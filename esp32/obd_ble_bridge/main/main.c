#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "error_snapshot.h"
#include "esp_spiffs.h"

// DICHIARA qui le funzioni del tuo BLE server
void ble_init(void);
void send_ble_notify(const char *data, size_t len);
extern bool device_connected;  // questa DEVE essere globale in ble_server.c

#define UART_PORT_NUM      UART_NUM_0
#define UART_BAUD_RATE     115200
#define BUF_SIZE           256

static const char *TAG = "MAIN";

// --- Nuova funzione parsing: estrae TUTTI i dati, anche veicolo ---
void parse_and_maybe_snapshot(const char* data) {
    char error_code[32] = "";
    int rpm = -1, speed = -1, temp = -1, fuel = -1, mil = -1;
    char marca[24] = "", modello[24] = "", vin[24] = "", targa[16] = "";
    long km = 0;

    // Estrazione campi veicolo
    const char* p;
    p = strstr(data, "MARCA:");
    if (p) sscanf(p, "MARCA:%23[^;]", marca);
    p = strstr(data, "MODELLO:");
    if (p) sscanf(p, "MODELLO:%23[^;]", modello);
    p = strstr(data, "VIN:");
    if (p) sscanf(p, "VIN:%23[^;]", vin);
    p = strstr(data, "TARGA:");
    if (p) sscanf(p, "TARGA:%15[^;]", targa);
    p = strstr(data, "KM:");
    if (p) sscanf(p, "KM:%ld", &km);

    // Estrazione campi OBD
    p = strstr(data, "ERROR_CODES:");
    if (p) sscanf(p, "ERROR_CODES:%31[^;]", error_code);
    p = strstr(data, "RPM:");
    if (p) sscanf(p, "RPM:%d", &rpm);
    p = strstr(data, "SPEED:");
    if (p) sscanf(p, "SPEED:%d", &speed);
    p = strstr(data, "TEMP:");
    if (p) sscanf(p, "TEMP:%d", &temp);
    p = strstr(data, "FUEL:");
    if (p) sscanf(p, "FUEL:%d", &fuel);
    p = strstr(data, "MIL:");
    if (p) sscanf(p, "MIL:%d", &mil);

    // Se c'è un errore significativo lo salvo con tutti i dati veicolo!
    if (strlen(error_code) > 0 && strcmp(error_code, "NONE") != 0) {
        error_snapshot_save(error_code, rpm, speed, temp, fuel, mil, marca, modello, vin, targa, km);

        ESP_LOGW(TAG, "!!! Snapshot errore salvato: codice=%s, rpm=%d, speed=%d, temp=%d, fuel=%d, mil=%d, marca=%s, modello=%s, vin=%s, targa=%s, km=%ld",
            error_code, rpm, speed, temp, fuel, mil, marca, modello, vin, targa, km);

        // Esempio: stampa tutti gli snapshot su seriale
        const error_snapshot_t* snap = error_snapshot_get(error_snapshot_get_count()-1);
        if (snap) {
            printf("Nuovo errore: %s @ %llu us, RPM: %d, Speed: %d, Temp: %d, Fuel: %d, MIL: %d\n"
                   "  Veicolo: %s %s, VIN: %s, Targa: %s, KM: %ld\n",
                snap->error_code, snap->timestamp_us, snap->rpm, snap->speed, snap->temp, snap->fuel, snap->mil,
                snap->marca, snap->modello, snap->vin, snap->targa, snap->km);
        }
    }
}

// Task bridge BLE<->seriale E BLE<->comandi speciali
void serial_ble_bridge_task(void *arg)
{
    uint8_t* data = (uint8_t*) malloc(BUF_SIZE);
    size_t len = 0;
    while (1) {
        int rxBytes = uart_read_bytes(UART_PORT_NUM, data + len, 1, pdMS_TO_TICKS(100));
        if (rxBytes > 0) {
            len += rxBytes;
            if (data[len-1] == '\n' || len >= (BUF_SIZE-1)) {
                data[len] = 0;
                ESP_LOGI(TAG, "RX: %s", data);

                if (device_connected) {
                    send_ble_notify((const char*)data, len);
                } else {
                    ESP_LOGW(TAG, "BLE non connesso, messaggio ignorato");
                }
                parse_and_maybe_snapshot((const char*)data);
                len = 0;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    free(data);
}

void app_main(void)
{
    // Inizializza SPIFFS
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 3,
      .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS Init fail (%d)", ret);
    } else {
        ESP_LOGI(TAG, "SPIFFS mounted OK");
    }

    // Inizializza BLE
    ble_init();

    // Configura UART
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    xTaskCreate(serial_ble_bridge_task, "serial_ble_bridge_task", 4096, NULL, 10, NULL);

    ESP_LOGI(TAG, "Bridge BLE<->Seriale avviato!");
}

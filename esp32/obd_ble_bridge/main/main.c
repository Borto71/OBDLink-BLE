#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

// Dichiarazioni esterne per BLE
extern void ble_init(void);
extern void send_ble_notify(const char *data, size_t len);

#define UART_NUM UART_NUM_0
#define BUF_SIZE 512

static const char *TAG = "OBD_RECEIVER";

#define MAX_ERROR_CODES 5
typedef struct {
    int rpm;
    int speed;
    int temp;
    float fuel;
    int mil;
    char error_codes[MAX_ERROR_CODES][8]; // Es. "P0420"
    int num_errori;
} obd_data_t;

void reset_obd_data(obd_data_t *data) {
    data->rpm = -1;
    data->speed = -1;
    data->temp = -1;
    data->fuel = -1;
    data->mil = -1;
    data->num_errori = 0;
    for (int i = 0; i < MAX_ERROR_CODES; i++)
        data->error_codes[i][0] = '\0';
}

void parse_and_print(const char *data, obd_data_t *obd) {
    char buffer[256];
    strncpy(buffer, data, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = 0;

    char *token = strtok(buffer, ";");
    while (token != NULL) {
        char *sep = strchr(token, ':');
        if (sep) {
            *sep = 0;
            const char *key = token;
            const char *value = sep + 1;

            if (strcmp(key, "RPM") == 0 && isdigit((unsigned char)value[0]))
                obd->rpm = atoi(value);
            else if (strcmp(key, "SPEED") == 0 && isdigit((unsigned char)value[0]))
                obd->speed = atoi(value);
            else if (strcmp(key, "TEMP") == 0 && isdigit((unsigned char)value[0]))
                obd->temp = atoi(value);
            else if (strcmp(key, "FUEL") == 0)
                obd->fuel = atof(value);
            else if (strcmp(key, "MIL") == 0 && (value[0] == '0' || value[0] == '1'))
                obd->mil = atoi(value);
            else if (strcmp(key, "ERROR_CODES") == 0) {
                if (strcmp(value, "NONE") == 0) {
                    obd->num_errori = 0;
                } else {
                    obd->num_errori = 0;
                    char *v = strdup(value); // copia modificabile
                    char *errtok = strtok(v, ",");
                    while (errtok != NULL && obd->num_errori < MAX_ERROR_CODES) {
                        strncpy(obd->error_codes[obd->num_errori], errtok, 8);
                        obd->error_codes[obd->num_errori][7] = '\0';
                        obd->num_errori++;
                        errtok = strtok(NULL, ",");
                    }
                    free(v);
                }
            } else {
                ESP_LOGW(TAG, "Campo sconosciuto: %s -> %s", key, value);
            }
        }
        token = strtok(NULL, ";");
    }

    ESP_LOGI(TAG, "=== Dati OBD ricevuti ===");
    if (obd->rpm != -1)     ESP_LOGI(TAG, "RPM: %d", obd->rpm);
    if (obd->speed != -1)   ESP_LOGI(TAG, "SPEED: %d", obd->speed);
    if (obd->temp != -1)    ESP_LOGI(TAG, "TEMP: %d", obd->temp);
    if (obd->fuel != -1)    ESP_LOGI(TAG, "FUEL: %.1f", obd->fuel);
    if (obd->mil != -1)     ESP_LOGI(TAG, "MIL: %d", obd->mil);
    if (obd->num_errori > 0) {
        ESP_LOGI(TAG, "ERROR_CODES:");
        for (int i = 0; i < obd->num_errori; i++)
            ESP_LOGI(TAG, "   [%d] %s", i + 1, obd->error_codes[i]);
    } else {
        ESP_LOGI(TAG, "ERROR_CODES: NONE");
    }
    ESP_LOGI(TAG, "=========================");
}

void obd_uart_task(void *arg)
{
    uint8_t* data = (uint8_t*) malloc(BUF_SIZE);
    int idx = 0;
    obd_data_t obd;
    reset_obd_data(&obd);

    while (1) {
        int len = uart_read_bytes(UART_NUM, data + idx, 1, 100 / portTICK_PERIOD_MS);
        if (len > 0) {
            if (data[idx] == '\n') {
                data[idx] = 0;  // termina la stringa
                ESP_LOGI(TAG, "Pacchetto ricevuto: %s", (char*)data);
                reset_obd_data(&obd);
                parse_and_print((char*)data, &obd);

                // Invia dati ricevuti via BLE al client connesso
                send_ble_notify((char*)data, strlen((char*)data));

                idx = 0; // reset buffer
            } else {
                if (++idx >= BUF_SIZE - 1) {
                    ESP_LOGW(TAG, "Buffer overflow, resetto buffer.");
                    idx = 0;
                }
            }
        }
    }
    free(data);
}

void app_main(void)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM, &uart_config);
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);

    xTaskCreate(obd_uart_task, "obd_uart_task", 4096, NULL, 10, NULL);

    ble_init();  // inizializza BLE server

    printf("Test print ESP32!\n");

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

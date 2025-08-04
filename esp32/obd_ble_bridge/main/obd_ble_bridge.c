#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

#define UART_NUM UART_NUM_0   // oppure UART_NUM_1 se usi RX/TX esterni
#define BUF_SIZE 1024

static const char *TAG = "OBD_RECEIVER";

void parse_and_print(const char *data) {
    char buffer[256];
    strncpy(buffer, data, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = 0;

    char *token = strtok(buffer, ";");
    while (token != NULL) {
        char *sep = strchr(token, ':');
        if (sep) {
            *sep = 0;
            const char *key = token;
            const char *value = sep + 1;
            ESP_LOGI(TAG, "Campo: %s  ->  Valore: %s", key, value);
        }
        token = strtok(NULL, ";");
    }
}

void obd_uart_task(void *arg)
{
    uint8_t* data = (uint8_t*) malloc(BUF_SIZE);
    int idx = 0;
    while (1) {
        int len = uart_read_bytes(UART_NUM, data + idx, 1, 100 / portTICK_PERIOD_MS);
        if (len > 0) {
            if (data[idx] == '\n') {
                data[idx] = 0;  // termina la stringa
                ESP_LOGI(TAG, "Pacchetto ricevuto: %s", (char*)data);
                parse_and_print((char*)data);
                idx = 0; // reset buffer
            } else {
                if (++idx >= BUF_SIZE - 1) {
                    idx = 0; // buffer overflow, reset
                }
            }
        }
    }
    free(data);
}

void app_main(void)
{
    // Configura UART (se necessario modifica i pin)
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
}

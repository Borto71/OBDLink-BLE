#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "error_snapshot.h"

#define TAG "BLE_MINIMAL"

// UUIDs
#define SERVICE_UUID        0xFF00
#define CHAR_UUID           0xFF01

// Variabili globali per mantenere riferimenti agli handle BLE
static uint16_t service_handle = 0;
static esp_gatt_if_t gatts_if_for_notify = 0;
static uint16_t conn_id = 0;
static uint16_t char_handle = 0;
bool device_connected = false; // Stato connessione

// GATT Database
static esp_gatts_attr_db_t gatt_db[] = {
    // Servizio primario
    [0] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){ESP_GATT_UUID_PRI_SERVICE}, ESP_GATT_PERM_READ,
         sizeof(uint16_t), sizeof(uint16_t), (uint8_t*)&(uint16_t){SERVICE_UUID}}
    },
    // Dichiarazione caratteristica (proprietà)
    [1] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){ESP_GATT_UUID_CHAR_DECLARE}, ESP_GATT_PERM_READ,
         sizeof(uint8_t), sizeof(uint8_t), (uint8_t*)&(uint8_t){ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY | ESP_GATT_CHAR_PROP_BIT_WRITE}}
    },
    // Valore caratteristica (dati veri e propri)
    [2] = {
        {ESP_GATT_RSP_BY_APP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){CHAR_UUID},
         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         240, 0, NULL}
    },
    // CCCD (Client Characteristic Configuration Descriptor) per abilitare notifiche
    [3] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){ESP_GATT_UUID_CHAR_CLIENT_CONFIG},
         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(uint16_t), sizeof(uint16_t), (uint8_t*)&(uint16_t){0x0000}}
    }
};

// Callback per eventi GAP (advertising, connessioni)
void ble_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: // Advertising avviato
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "BLE advertising started");
            } else {
                ESP_LOGE(TAG, "BLE advertising start failed");
            }
            break;
        default:
            break;
    }
}

// Prototipo funzione per invio notifiche
void send_ble_notify(const char *data, size_t len);

// Callback per eventi GATT Server
void ble_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                            esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT: { // Registrazione applicazione BLE completata
            ESP_LOGI(TAG, "ESP_GATTS_REG_EVT");
            esp_ble_gap_set_device_name("ESP32_OBD_BLE"); // Nome dispositivo BLE

            // UUID a 128 bit (necessario per compatibilità con Chrome/Web Bluetooth)
            static uint8_t adv_service_uuid128[16] = {
                0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                0x00, 0x10, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00
            };

            // Configurazione advertising
            esp_ble_adv_data_t adv_data = {
                .set_scan_rsp = false,
                .include_txpower = false,
                .service_uuid_len = sizeof(adv_service_uuid128),
                .p_service_uuid = adv_service_uuid128,
                .flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
            };
            esp_ble_gap_config_adv_data(&adv_data);
            // Creazione tabella attributi GATT
            esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, 4, 0);
            break;
        }

        case ESP_GATTS_CREAT_ATTR_TAB_EVT: // Creazione tabella attributi completata
            if (param->add_attr_tab.status == ESP_GATT_OK) {
                service_handle = param->add_attr_tab.handles[0];
                char_handle = param->add_attr_tab.handles[2];
                esp_ble_gatts_start_service(service_handle); // Avvio servizio

                // Avvio advertising
                esp_ble_gap_start_advertising(&(esp_ble_adv_params_t){
                    .adv_int_min = 0x20,
                    .adv_int_max = 0x40,
                    .adv_type = ADV_TYPE_IND,
                    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
                    .channel_map = ADV_CHNL_ALL,
                    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
                });
            } else {
                ESP_LOGE(TAG, "Create attribute table failed, error code=0x%x", param->add_attr_tab.status);
            }
            break;

        case ESP_GATTS_CONNECT_EVT: // Dispositivo connesso
            ESP_LOGI(TAG, "Device connected");
            gatts_if_for_notify = gatts_if;
            conn_id = param->connect.conn_id; // Salva interfaccia per notifiche
            device_connected = true;
            break;

        case ESP_GATTS_DISCONNECT_EVT: // Dispositivo disconnesso
            ESP_LOGI(TAG, "Device disconnected, restart advertising");
            device_connected = false;

            // Riavvia advertising
            esp_ble_gap_start_advertising(&(esp_ble_adv_params_t){
                .adv_int_min = 0x20,
                .adv_int_max = 0x40,
                .adv_type = ADV_TYPE_IND,
                .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
                .channel_map = ADV_CHNL_ALL,
                .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
            });
            break;

        case ESP_GATTS_READ_EVT: // Lettura caratteristica
            ESP_LOGD(TAG, "ESP_GATTS_READ_EVT");
            break;

        case ESP_GATTS_WRITE_EVT: // Scrittura caratteristica/CCCD
            // Gestione CCCD (attivazione notifiche)
            if (param->write.handle == char_handle + 1 && param->write.len == 2) {
                uint16_t cccd = param->write.value[1] << 8 | param->write.value[0];
                ESP_LOGI(TAG, "CCCD write, value: 0x%04X", cccd);

                // Scrittura valore caratteristica (comando custom)
            } else if (param->write.handle == char_handle) {
                // Comando custom ricevuto dalla web app
                char cmd[32] = {0}; // Buffer comando
                int len = param->write.len;
                if (len >= sizeof(cmd)) len = sizeof(cmd)-1;
                memcpy(cmd, param->write.value, len);

                ESP_LOGI(TAG, "Comando BLE ricevuto: %s", cmd);

                if (strncmp(cmd, "GET_ALL_ERRORS", strlen("GET_ALL_ERRORS")) == 0) {
                    // Usa buffer statico per evitare overflow dello stack
                    static error_snapshot_t snapshots[MAX_ERROR_SNAPSHOTS];
                    int n = error_snapshot_load_all_from_file(snapshots, MAX_ERROR_SNAPSHOTS);
                    for (int i = 0; i < n; ++i) {
                        char buf[240];
                        snprintf(buf, sizeof(buf), "%llu,%s,%d,%d,%d,%d,%d,%s,%s,%s,%s,%ld",
                            snapshots[i].timestamp_us, snapshots[i].error_code,
                            snapshots[i].rpm, snapshots[i].speed, snapshots[i].temp,
                            snapshots[i].fuel, snapshots[i].mil,
                            snapshots[i].marca, snapshots[i].modello, snapshots[i].vin,
                            snapshots[i].targa, snapshots[i].km);
                        send_ble_notify(buf, strlen(buf)); // Invio BLE
                        vTaskDelay(pdMS_TO_TICKS(80)); // Pausa per evitare congestione BLE
                    }
                    // INVIA SEGNALE DI FINE!
                    send_ble_notify("END_OF_ERRORS", strlen("END_OF_ERRORS"));
                } else if (strncmp(cmd, "RESET_ERRORS", strlen("RESET_ERRORS")) == 0) {
                    error_snapshot_clear_file();
                    ESP_LOGI(TAG, "Tutti gli errori sono stati resettati");
                    send_ble_notify("ERRORS_CLEARED", strlen("ERRORS_CLEARED"));
                }
            }
            break;

        // Eventi non usati
        case ESP_GATTS_EXEC_WRITE_EVT:
        case ESP_GATTS_MTU_EVT:
        case ESP_GATTS_CONF_EVT:
        case ESP_GATTS_UNREG_EVT:
        case ESP_GATTS_CREATE_EVT:
        case ESP_GATTS_ADD_INCL_SRVC_EVT:
        case ESP_GATTS_ADD_CHAR_EVT:
        case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        case ESP_GATTS_DELETE_EVT:
        case ESP_GATTS_START_EVT:
        case ESP_GATTS_STOP_EVT:
        case ESP_GATTS_OPEN_EVT:
        case ESP_GATTS_CANCEL_OPEN_EVT:
        case ESP_GATTS_CLOSE_EVT:
        case ESP_GATTS_LISTEN_EVT:
        case ESP_GATTS_CONGEST_EVT:
        case ESP_GATTS_RESPONSE_EVT:
        case ESP_GATTS_SET_ATTR_VAL_EVT:
        case ESP_GATTS_SEND_SERVICE_CHANGE_EVT:
            break;

        default:
            break;
    }
}

// === Funzione di inizializzazione BLE ===

void ble_init(void) {
    esp_err_t ret;

    // Rilascia memoria Bluetooth classico
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    // Inizializza controller BLE
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    // Registra callback GATT e GAP
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(ble_gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(ble_gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(0)); // ID app GATT
}

// Invio notifiche BLE
void send_ble_notify(const char *data, size_t len) {
    if (!device_connected) return;  // Nessuna connessione, esce
    if (!data || len == 0 || len > 230) return; // Controllo validità dati

    esp_err_t ret = esp_ble_gatts_send_indicate(
        gatts_if_for_notify, conn_id, char_handle, len, (uint8_t*)data, false
    );
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "BLE notify sent: %.*s", (int)len, data);
    } else {
        ESP_LOGE(TAG, "BLE notify failed, err=0x%x", ret);
    }
}

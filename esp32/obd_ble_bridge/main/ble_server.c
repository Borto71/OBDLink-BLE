#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#define TAG "BLE_MINIMAL"

// UUIDs
#define SERVICE_UUID        0xFF00
#define CHAR_UUID           0xFF01

static uint16_t service_handle = 0;
static esp_gatt_if_t gatts_if_for_notify = 0;
static uint16_t conn_id = 0;
static uint16_t char_handle = 0;
bool device_connected = false;

static esp_gatts_attr_db_t gatt_db[] = {
    // Primary Service
    [0] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){ESP_GATT_UUID_PRI_SERVICE}, ESP_GATT_PERM_READ,
         sizeof(uint16_t), sizeof(uint16_t), (uint8_t*)&(uint16_t){SERVICE_UUID}}
    },
    // Characteristic Declaration
    [1] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){ESP_GATT_UUID_CHAR_DECLARE}, ESP_GATT_PERM_READ,
         sizeof(uint8_t), sizeof(uint8_t), (uint8_t*)&(uint8_t){ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY}}
    },
    // Characteristic Value
    [2] = {
        {ESP_GATT_RSP_BY_APP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){CHAR_UUID},
         ESP_GATT_PERM_READ,
         128, 0, NULL}
    },
    // Client Characteristic Configuration Descriptor (CCCD)
    [3] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){ESP_GATT_UUID_CHAR_CLIENT_CONFIG},
         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(uint16_t), sizeof(uint16_t), (uint8_t*)&(uint16_t){0x0000}}
    }
};

void ble_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
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

void ble_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                            esp_ble_gatts_cb_param_t *param) {
    switch (event) {
case ESP_GATTS_REG_EVT: {
    ESP_LOGI(TAG, "ESP_GATTS_REG_EVT");
    esp_ble_gap_set_device_name("ESP32_OBD_BLE");
    
    // UUID 128bit per Chrome
    static uint8_t adv_service_uuid128[16] = {
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
        0x00, 0x10, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00
    };
    esp_ble_adv_data_t adv_data = {
        .set_scan_rsp = false,
        .include_txpower = false,
        .service_uuid_len = sizeof(adv_service_uuid128),
        .p_service_uuid = adv_service_uuid128,
        .flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
    };
    esp_ble_gap_config_adv_data(&adv_data);
    esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, 4, 0);
    break;
}


        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            if (param->add_attr_tab.status == ESP_GATT_OK) {
                service_handle = param->add_attr_tab.handles[0];
                char_handle = param->add_attr_tab.handles[2];
                esp_ble_gatts_start_service(service_handle);
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

        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(TAG, "Device connected");
            gatts_if_for_notify = gatts_if;
            conn_id = param->connect.conn_id;
            device_connected = true;
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(TAG, "Device disconnected, restart advertising");
            device_connected = false;
            esp_ble_gap_start_advertising(&(esp_ble_adv_params_t){
                .adv_int_min = 0x20,
                .adv_int_max = 0x40,
                .adv_type = ADV_TYPE_IND,
                .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
                .channel_map = ADV_CHNL_ALL,
                .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
            });
            break;

        case ESP_GATTS_READ_EVT:
            ESP_LOGD(TAG, "ESP_GATTS_READ_EVT");
            break;

        case ESP_GATTS_WRITE_EVT:
            // Gestione CCCD
            if (param->write.handle == char_handle + 1 && param->write.len == 2) {
                uint16_t cccd = param->write.value[1] << 8 | param->write.value[0];
                ESP_LOGI(TAG, "CCCD write, value: 0x%04X", cccd);
            }
            break;

        default:
            break;
    }
}

// === API da chiamare dal main ===

void ble_init(void) {
    esp_err_t ret;

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(ble_gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(ble_gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(0));
}

void send_ble_notify(const char *data, size_t len) {
    if (!device_connected) return;
    if (!data || len == 0 || len > 128) return;

    esp_err_t ret = esp_ble_gatts_send_indicate(
        gatts_if_for_notify, conn_id, char_handle, len, (uint8_t*)data, false
    );
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "BLE notify sent: %.*s", (int)len, data);
    } else {
        ESP_LOGE(TAG, "BLE notify failed, err=0x%x", ret);
    }
}

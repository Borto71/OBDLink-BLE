#include <string.h>
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#define TAG "BLE_SERVER"

#define SERVICE_UUID        0x00FF
#define CHARACTERISTIC_UUID 0xFF01

static uint16_t service_handle = 0;
static uint16_t char_handle = 0;
static esp_gatt_if_t gatts_if_global = 0;
static uint16_t conn_id_global = 0;
static bool device_connected = false;

static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
                                        esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param);

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch(event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            esp_ble_gap_start_advertising(&(esp_ble_adv_params_t){
                .adv_int_min = 0x20,
                .adv_int_max = 0x40,
                .adv_type = ADV_TYPE_IND,
                .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
                .channel_map = ADV_CHNL_ALL,
                .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY
            });
            break;
        default:
            break;
    }
}

// --- Costanti GATT necessarie per la tabella attributi (gatt_db)
static const uint16_t primary_service_uuid       = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint8_t char_prop_notify            = ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static uint16_t service_uuid                     = SERVICE_UUID;
static uint16_t characteristic_uuid              = CHARACTERISTIC_UUID;

// --- Struttura service_id aggiornata per ESP-IDF 5/6
static esp_gatt_srvc_id_t service_id = {
    .is_primary = true,
    .id = {
        .inst_id = 0,
        .uuid = {
            .len = ESP_UUID_LEN_16,
            .uuid = {.uuid16 = SERVICE_UUID}
        }
    }
};

// --- GATT attribute database (puoi usarla se vuoi tabella attributi custom)
static esp_gatts_attr_db_t gatt_db[] = {
    // Service Declaration
    [0] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
         sizeof(uint16_t), sizeof(uint16_t), (uint8_t *)&service_uuid}
    },
    // Characteristic Declaration
    [1] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
         sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_notify}
    },
    // Characteristic Value
    [2] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&characteristic_uuid, ESP_GATT_PERM_READ,
         512, 0, NULL}
    },
};

void ble_init(void) {
    esp_err_t ret;

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    ESP_ERROR_CHECK(ret);

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    ESP_ERROR_CHECK(ret);

    ret = esp_bluedroid_init();
    ESP_ERROR_CHECK(ret);

    ret = esp_bluedroid_enable();
    ESP_ERROR_CHECK(ret);

    ret = esp_ble_gap_register_callback(gap_event_handler);
    ESP_ERROR_CHECK(ret);

    ret = esp_ble_gatts_register_callback(gatts_profile_event_handler);
    ESP_ERROR_CHECK(ret);

    ret = esp_ble_gatts_app_register(0);
    ESP_ERROR_CHECK(ret);
}

void send_ble_notify(const char *data, size_t len) {
    if (device_connected) {
        esp_ble_gatts_send_indicate(gatts_if_global, conn_id_global, char_handle,
                                    len, (uint8_t *)data, false);
        ESP_LOGI(TAG, "Inviata notifica BLE: %.*s", (int)len, data);
    }
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
                                        esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param) {
    switch(event) {
    case ESP_GATTS_REG_EVT:
        esp_ble_gap_set_device_name("OBD_BLE");
        esp_ble_gap_config_adv_data(&(esp_ble_adv_data_t){
            .set_scan_rsp = false,
            .include_name = true,
            .appearance = 0x00,
            .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
        });
        esp_ble_gatts_create_service(gatts_if, &service_id, 4);
        break;
    case ESP_GATTS_CREATE_EVT:
        service_handle = param->create.service_handle;
        esp_ble_gatts_start_service(service_handle);
        esp_ble_gatts_add_char(service_handle, &(esp_bt_uuid_t){
            .len=ESP_UUID_LEN_16,
            .uuid={.uuid16=CHARACTERISTIC_UUID}
        },
        ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_NOTIFY, NULL, NULL);
        break;
    case ESP_GATTS_ADD_CHAR_EVT:
        char_handle = param->add_char.attr_handle;
        break;
    case ESP_GATTS_CONNECT_EVT:
        gatts_if_global = gatts_if;
        conn_id_global = param->connect.conn_id;
        device_connected = true;
        ESP_LOGI(TAG, "Dispositivo connesso");
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        device_connected = false;
        ESP_LOGI(TAG, "Dispositivo disconnesso");
        esp_ble_gap_start_advertising(&(esp_ble_adv_params_t){
            .adv_int_min = 0x20,
            .adv_int_max = 0x40,
            .adv_type = ADV_TYPE_IND,
            .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
            .channel_map = ADV_CHNL_ALL,
            .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY
        });
        break;
    default:
        break;
    }
}

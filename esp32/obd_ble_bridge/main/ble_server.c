#include <string.h>
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#define TAG "BLE_MINIMAL"
#define SERVICE_UUID        0x00FF
#define CHAR_UUID           0xFF01

static uint16_t service_handle = 0;
static uint16_t char_handle = 0;
static esp_gatt_if_t gatts_if_for_notify = 0;
static uint16_t conn_id_for_notify = 0;
static bool is_connected = false;

// PROTOTYPE
static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
                                        esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param);

static void start_advertising() {
    esp_ble_gap_start_advertising(&(esp_ble_adv_params_t){
        .adv_int_min = 0x20,
        .adv_int_max = 0x40,
        .adv_type = ADV_TYPE_IND,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .channel_map = ADV_CHNL_ALL,
        .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY
    });
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            // NO start advertising qui!
            break;
        default:
            break;
    }
}

// ********* CHIAMALA IN app_main() dopo nvs_flash_init()! *********
void ble_init(void) {
    esp_err_t ret;
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg); ESP_ERROR_CHECK(ret);
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE); ESP_ERROR_CHECK(ret);
    ret = esp_bluedroid_init(); ESP_ERROR_CHECK(ret);
    ret = esp_bluedroid_enable(); ESP_ERROR_CHECK(ret);

    ret = esp_ble_gap_register_callback(gap_event_handler); ESP_ERROR_CHECK(ret);
    ret = esp_ble_gatts_register_callback(gatts_profile_event_handler); ESP_ERROR_CHECK(ret);
    ret = esp_ble_gatts_app_register(0); ESP_ERROR_CHECK(ret);
}

// ********* CHIAMALA da main.c per inviare notifiche! *********
void send_ble_notify(const char *data, size_t len) {
    if (is_connected && char_handle && gatts_if_for_notify) {
        esp_ble_gatts_set_attr_value(char_handle, len, (const uint8_t *)data);
        esp_ble_gatts_send_indicate(gatts_if_for_notify, conn_id_for_notify, char_handle, len, (uint8_t *)data, false);
        ESP_LOGI(TAG, "BLE notify sent: %.*s", (int)len, data);
    }
}

// =============== CORE HANDLER ================
static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
                                        esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        esp_ble_gap_set_device_name("OBD_BLE");
        esp_ble_gap_config_adv_data(&(esp_ble_adv_data_t){
            .set_scan_rsp = false,
            .include_name = true,
            .appearance = 0x00,
            .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
        });

        esp_ble_gatts_create_service(gatts_if, &(esp_gatt_srvc_id_t){
            .is_primary = true,
            .id = {
                .inst_id = 0,
                .uuid = {
                    .len = ESP_UUID_LEN_16,
                    .uuid = {.uuid16 = SERVICE_UUID}
                }
            }
        }, 4);
        break;
    case ESP_GATTS_CREATE_EVT:
        service_handle = param->create.service_handle;
        esp_ble_gatts_start_service(service_handle);

        esp_ble_gatts_add_char(service_handle, &(esp_bt_uuid_t){
            .len = ESP_UUID_LEN_16,
            .uuid = {.uuid16 = CHAR_UUID}
        },
        ESP_GATT_PERM_READ,
        ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
        NULL, NULL);
        break;
    case ESP_GATTS_ADD_CHAR_EVT:
        char_handle = param->add_char.attr_handle;
        // Aggiungi manualmente CCCD
        esp_ble_gatts_add_char_descr(service_handle, &(esp_bt_uuid_t){
            .len = ESP_UUID_LEN_16,
            .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG}
        },
        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
        break;
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        // SOLO ORA avvia advertising!
        start_advertising();
        break;
    case ESP_GATTS_CONNECT_EVT:
        is_connected = true;
        gatts_if_for_notify = gatts_if;
        conn_id_for_notify = param->connect.conn_id;
        ESP_LOGI(TAG, "Device connected");
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        is_connected = false;
        ESP_LOGI(TAG, "Device disconnected, restart advertising");
        start_advertising();
        break;
    default:
        break;
    }
}

#include "ble.h"
#include "internal_flash.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"

#define BLE_DEVICE_NAME          "ESP32_PRINTER"
#define BLE_APP_ID               0x55
#define BLE_SERVICE_UUID         0x00FF
#define BLE_SSID_CHAR_UUID       0xFF01
#define BLE_PASSWORD_CHAR_UUID   0xFF02
#define BLE_NUM_HANDLE           8


static uint8_t ssid_flag = 0;
static uint8_t pass_flag = 0;

static const char *TAG = "ble";

static uint16_t service_handle;
static uint16_t ssid_char_handle;
static uint16_t pass_char_handle;
static uint16_t conn_id;
static esp_gatt_if_t notify_gatts_if = ESP_GATT_IF_NONE;
static bool ble_connected = false;
static bool ble_adv_enabled = true;
static volatile bool wifi_config_done = false;

static char wifi_ssid[32];
static char wifi_pass[64];

static uint8_t ssid_value[32] = "";
static uint8_t pass_value[64] = "";

static esp_gatt_srvc_id_t service_id = {
    .is_primary = true,
    .id.inst_id = 0,
    .id.uuid.len = ESP_UUID_LEN_16,
    .id.uuid.uuid.uuid16 = BLE_SERVICE_UUID,
};

static esp_bt_uuid_t ssid_char_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid.uuid16 = BLE_SSID_CHAR_UUID,
};

static esp_bt_uuid_t pass_char_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid.uuid16 = BLE_PASSWORD_CHAR_UUID,
};

static esp_attr_value_t ssid_char_val = {
    .attr_max_len = sizeof(ssid_value),
    .attr_len = 1,
    .attr_value = ssid_value,
};

static esp_attr_value_t pass_char_val = {
    .attr_max_len = sizeof(pass_value),
    .attr_len = 1,
    .attr_value = pass_value,
};

static esp_attr_control_t char_control = {
    .auto_rsp = ESP_GATT_AUTO_RSP,
};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void copy_write_value(char *dst, size_t dst_size, const uint8_t *src, uint16_t src_len)
{
    size_t copy_len = src_len;

    if (copy_len >= dst_size) {
        copy_len = dst_size - 1;
    }

    memset(dst, 0, dst_size);
    memcpy(dst, src, copy_len);
}

static void gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        if (ble_adv_enabled) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "advertising started");
        } else {
            ESP_LOGE(TAG, "advertising start failed");
        }
        break;

    default:
        break;
    }
}

static void gatts_callback(esp_gatts_cb_event_t event,
                           esp_gatt_if_t gatts_if,
                           esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(TAG, "GATT registered");
        esp_ble_gap_set_device_name(BLE_DEVICE_NAME);
        esp_ble_gap_config_adv_data(&adv_data);
        esp_ble_gatts_create_service(gatts_if, &service_id, BLE_NUM_HANDLE);
        break;

    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(TAG, "service created");
        service_handle = param->create.service_handle;
        esp_ble_gatts_start_service(service_handle);
        esp_ble_gatts_add_char(service_handle,
                               &ssid_char_uuid,
                               ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                               ESP_GATT_CHAR_PROP_BIT_READ |
                                   ESP_GATT_CHAR_PROP_BIT_WRITE |
                                   ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                               &ssid_char_val,
                               &char_control);
        break;

    case ESP_GATTS_ADD_CHAR_EVT:
        if (param->add_char.char_uuid.uuid.uuid16 == BLE_SSID_CHAR_UUID) {
            ESP_LOGI(TAG, "SSID characteristic added");
            ssid_char_handle = param->add_char.attr_handle;
            esp_ble_gatts_add_char(service_handle,
                                   &pass_char_uuid,
                                   ESP_GATT_PERM_WRITE,
                                   ESP_GATT_CHAR_PROP_BIT_WRITE,
                                   &pass_char_val,
                                   &char_control);
        } else if (param->add_char.char_uuid.uuid.uuid16 == BLE_PASSWORD_CHAR_UUID) {
            ESP_LOGI(TAG, "password characteristic added");
            pass_char_handle = param->add_char.attr_handle;
        }
        break;

    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(TAG, "client connected");
        ble_connected = true;
        conn_id = param->connect.conn_id;
        notify_gatts_if = gatts_if;
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "client disconnected");
        ble_connected = false;
        notify_gatts_if = ESP_GATT_IF_NONE;
        if (ble_adv_enabled) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;

    case ESP_GATTS_WRITE_EVT:
        ESP_LOGI(TAG,
                "WRITE_EVT handle=%d, ssid_handle=%d, pass_handle=%d, len=%d",
                param->write.handle,
                ssid_char_handle,
                pass_char_handle,
                param->write.len);
        if (param->write.handle == ssid_char_handle) {
            memset(wifi_ssid, 0, sizeof(wifi_ssid));
            copy_write_value(wifi_ssid,
                            sizeof(wifi_ssid),
                            param->write.value,
                            param->write.len);

            ESP_LOGI(TAG, "SSID: %s", wifi_ssid);
            ssid_flag = 1;
        } else if (param->write.handle == pass_char_handle) {
            memset(wifi_pass, 0, sizeof(wifi_pass));
            copy_write_value(wifi_pass,
                            sizeof(wifi_pass),
                            param->write.value,
                            param->write.len);

            ESP_LOGI(TAG, "password received");
            pass_flag = 1;
        } else {
            ESP_LOGW(TAG, "unknown write handle");
        }

        ESP_LOGI(TAG, "flags: ssid=%d pass=%d", ssid_flag, pass_flag);

        if ((ssid_flag == 1) && (pass_flag == 1)) {
            ESP_LOGI(TAG, "writing to flash");

            save_wifi_config(wifi_ssid, wifi_pass);
            wifi_config_done = true;

            ssid_flag = 0;
            pass_flag = 0;
        }
        break;

    default:
        break;
    }
}

void ble_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_callback));
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_callback));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(BLE_APP_ID));
}

void ble_notify(const uint8_t *data, uint16_t len)
{
    if (!ble_connected || notify_gatts_if == ESP_GATT_IF_NONE || ssid_char_handle == 0) {
        return;
    }

    esp_ble_gatts_send_indicate(notify_gatts_if,
                                conn_id,
                                ssid_char_handle,
                                len,
                                (uint8_t *)data,
                                false);
}

const char *ble_get_ssid(void)
{
    return wifi_ssid;
}

const char *ble_get_pass(void)
{
    return wifi_pass;
}


void ble_start_advertising(void)
{
    ble_adv_enabled = true;
    esp_ble_gap_start_advertising(&adv_params);
}

void ble_stop_advertising(void)
{
    ble_adv_enabled = false;
    esp_ble_gap_stop_advertising();
}
bool ble_wifi_config_done(void)
{
    return wifi_config_done;
}

void ble_clear_wifi_config_done(void)
{
    wifi_config_done = false;
}


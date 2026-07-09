#include "wifi.h"
#include "ble.h"

#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"

static const char *TAG = "wifi";

static bool s_wifi_inited = false;
static bool s_wifi_started = false;

static bool s_wifi_retry = false;

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "station started, connecting...");
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        static int retry_count = 0;
        wifi_event_sta_disconnected_t *disc = (wifi_event_sta_disconnected_t *)event_data;

        ESP_LOGW(TAG, "wifi disconnected, reason=%d, retry=%d", disc->reason, retry_count);
        if (retry_count < 5) {
            retry_count++;
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "Wi-Fi connect failed, wait for reconfig");
            retry_count = 0;
            s_wifi_retry = true;

            esp_wifi_stop();
            s_wifi_started = false;

            ble_start_advertising();
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
        return;
    }
}

void wifi_init_sta(const char *ssid, const char *pass)
{
    if (!s_wifi_inited) {
        ESP_ERROR_CHECK(esp_netif_init());

        esp_err_t ret = esp_event_loop_create_default();
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_ERROR_CHECK(ret);
        }

        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &wifi_event_handler,
                                                            NULL,
                                                            NULL));

        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                            IP_EVENT_STA_GOT_IP,
                                                            &wifi_event_handler,
                                                            NULL,
                                                            NULL));

        s_wifi_inited = true;
    }

    wifi_config_t wifi_config = {0};

    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password) - 1);

    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    ESP_LOGI(TAG, "connect config ssid='%s', pass_len=%u", ssid, (unsigned)strlen(pass));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    if (!s_wifi_started) {
        ESP_ERROR_CHECK(esp_wifi_start());
        s_wifi_started = true;
    } else {
        esp_wifi_disconnect();
        esp_wifi_connect();
    }
}

void wifi_stop_sta(void)
{
    if (!s_wifi_inited || !s_wifi_started) {
        return;
    }

    esp_wifi_disconnect();
    esp_wifi_stop();
    s_wifi_started = false;
}


bool get_wifi_retry (void){

    return s_wifi_retry;
}
void clear_wifi_retry(void)
{
    s_wifi_retry = false;
}
#ifndef __APP_H__
#define __APP_H__

typedef enum {
    APP_STATE_BOOT = 0,
    APP_STATE_LOAD_CONFIG,
    APP_STATE_BLE_CONFIG,
    APP_STATE_WIFI_CONNECTING,
    APP_STATE_WIFI_CONNECTED,
    APP_STATE_MQTT_CONNECTING,
    APP_STATE_MQTT_CONNECTED,
    APP_STATE_OTA_CHECK,
    APP_STATE_OTA_RUNNING,
    APP_STATE_ERROR,
} app_state_t;


typedef struct {
    char wifi_ssid[32];
    char wifi_pass[64];
} app_config_t;

typedef struct {
    app_state_t state;
    app_config_t config;
} app_context_t;


void app_loop(void);
void app_process_state(void);

#endif // __APP_H__

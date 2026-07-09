#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "button.h"
#include "ble.h"
#include "internal_flash.h"
#include "wifi.h"
#include "app.h"
#include "led.h"
#include "mqtt.h"

#define APP_TASK_STACK_SIZE      4096
#define BUTTON_TASK_STACK_SIZE   2048
#define LED_TASK_STACK_SIZE      2048

#define APP_TASK_PRIORITY        4
#define BUTTON_TASK_PRIORITY     5
#define LED_TASK_PRIORITY     3

app_context_t app_ctx;

static int wifi_started = 0;
static int ble_inited = 0;
static int ble_advertising = 0;

static int mqtt_started = 0;

static void app_enter_ble_config(void)
{
    printf("enter BLE config mode\n");

    wifi_stop_sta();
    wifi_started = 0;

    memset(app_ctx.config.wifi_ssid, 0, sizeof(app_ctx.config.wifi_ssid));
    memset(app_ctx.config.wifi_pass, 0, sizeof(app_ctx.config.wifi_pass));

    if (ble_inited == 0) {
        
        ble_init();
        ble_inited = 1;
        ble_advertising = 1;
    } else if (ble_advertising == 0) {
        
        ble_start_advertising();
        ble_advertising = 1;
    }

    app_ctx.state = APP_STATE_BLE_CONFIG;
}

static void app_task(void *arg)
{
    memset(&app_ctx, 0, sizeof(app_ctx));
    app_ctx.state = APP_STATE_BOOT;

    while (1) {
        app_process_state();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void led_task(void *arg)
{
    while (1) {
        // printf("ble_inited %d ble_advertising %d \r\n", ble_inited, ble_advertising);
        if((ble_inited == 1) && (ble_advertising == 1)){
            led_blink();
        }
        else{
            led_on();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void button_task(void *arg)
{
    while (1) {
        if (button_is_pressed()) {
            int hold_time = button_hold();

            if (hold_time >= 300) {
                
                printf("button hold 3s\n");
                erase_wifi_config();
                ble_clear_wifi_config_done();
                app_enter_ble_config();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_start(void)
{
    xTaskCreate(app_task, "app_task", APP_TASK_STACK_SIZE, NULL, APP_TASK_PRIORITY, NULL);
    xTaskCreate(button_task, "button_task", BUTTON_TASK_STACK_SIZE, NULL, BUTTON_TASK_PRIORITY, NULL);
    xTaskCreate(led_task, "led_task", LED_TASK_STACK_SIZE, NULL, LED_TASK_PRIORITY, NULL);
}

void app_loop(void)
{
    app_start();
}

void app_process_state(void)
{
    switch (app_ctx.state) {
    case APP_STATE_BOOT:
        load_wifi_config(app_ctx.config.wifi_ssid,
                         sizeof(app_ctx.config.wifi_ssid),
                         app_ctx.config.wifi_pass,
                         sizeof(app_ctx.config.wifi_pass));

        if (app_ctx.config.wifi_ssid[0] != '\0' &&
            app_ctx.config.wifi_pass[0] != '\0') {
            app_ctx.state = APP_STATE_WIFI_CONNECTING;
        } else {
            app_enter_ble_config();
        }
        break;

    case APP_STATE_BLE_CONFIG:
        if (ble_wifi_config_done()) {
            ble_clear_wifi_config_done();

            load_wifi_config(app_ctx.config.wifi_ssid,
                             sizeof(app_ctx.config.wifi_ssid),
                             app_ctx.config.wifi_pass,
                             sizeof(app_ctx.config.wifi_pass));

            printf("wifi config done\n");
            ble_stop_advertising();
            ble_advertising = 0;
            app_ctx.state = APP_STATE_WIFI_CONNECTING;
        }
        break;

    case APP_STATE_WIFI_CONNECTING:
        if (wifi_started == 0) {
            
            printf("next to wifi\n");
            if (ble_inited) {
                ble_stop_advertising();
                ble_advertising = 0;
            }
            wifi_init_sta(app_ctx.config.wifi_ssid, app_ctx.config.wifi_pass);
            wifi_started = 1;
        }
        app_ctx.state = APP_STATE_WIFI_CONNECTED;
        break;

    case APP_STATE_WIFI_CONNECTED:
          
        if(get_wifi_retry()){
            wifi_started = 0;
            clear_wifi_retry();
            
            if (ble_inited == 0) {
                ble_init();
                ble_inited = 1;
                ble_advertising = 1;
            } else if (ble_advertising == 0) {
                ble_start_advertising();
                ble_advertising = 1;
            }
            app_ctx.state = APP_STATE_BLE_CONFIG;
        }
        // printf("connect mqtt \r\n");
        app_ctx.state = APP_STATE_MQTT_CONNECTING;
        break;

    case APP_STATE_MQTT_CONNECTING:
            if(get_ip()){
                clear_get_ip();
                printf("check mqtt\r\n");
                mqtt_app_init(URL_BROKER);
                app_ctx.state = APP_STATE_MQTT_CONNECTED;
            }
        break;

    case APP_STATE_MQTT_CONNECTED:
        // mqtt_app_subscribe(TOPIC);
        break;
    default:
        break;
    }
}


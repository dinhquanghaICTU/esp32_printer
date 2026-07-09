#include "mqtt.h"
#include "config.h"
#include "led.h"
#include "ota.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "mqtt_client.h"

#define MQTT_OTA_TOPIC "printer/ota"

static const char *TAG = "mqtt";

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_mqtt_connected = false;

static bool topic_match(esp_mqtt_event_handle_t event, const char *topic)
{
    return event->topic_len == strlen(topic) &&
           strncmp(event->topic, topic, event->topic_len) == 0;
}

static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "connected");
        s_mqtt_connected = true;

        esp_mqtt_client_subscribe(s_mqtt_client, TOPIC, 1);
        esp_mqtt_client_subscribe(s_mqtt_client, MQTT_OTA_TOPIC, 1);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "disconnected");
        s_mqtt_connected = false;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "subscribed msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "published msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "data received");
        printf("topic: %.*s\n", event->topic_len, event->topic);

        if (topic_match(event, MQTT_OTA_TOPIC)) {
            if (!ota_mqtt_submit_packet((const uint8_t *)event->data, event->data_len)) {
                ESP_LOGE(TAG, "submit OTA packet failed");
            }
        } else {
            printf("data : %.*s\n", event->data_len, event->data);
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "error");
        break;

    default:
        break;
    }
}

void mqtt_app_init(const char *url_broker)
{
    printf("check broker %s\r\n", url_broker);

    if (s_mqtt_client != NULL) {
        return;
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = url_broker,
        .session.keepalive = 60,
        .network.timeout_ms = 10000,
        .network.reconnect_timeout_ms = 5000,
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(s_mqtt_client,
                                   ESP_EVENT_ANY_ID,
                                   mqtt_event_handler,
                                   NULL);

    esp_mqtt_client_start(s_mqtt_client);
}

void mqtt_app_publish(const char *topic, const char *data)
{
    if (s_mqtt_client == NULL || !s_mqtt_connected) {
        ESP_LOGW(TAG, "mqtt not connected");
        return;
    }

    esp_mqtt_client_publish(s_mqtt_client,
                            topic,
                            data,
                            0,
                            1,
                            0);
}

void mqtt_app_subscribe(const char *topic)
{
    if (s_mqtt_client == NULL || !s_mqtt_connected) {
        ESP_LOGW(TAG, "mqtt not connected");
        return;
    }

    esp_mqtt_client_subscribe(s_mqtt_client, topic, 1);
}

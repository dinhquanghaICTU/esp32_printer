#include "ota.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "uart.h"

#define STM_OTA_TIMEOUT_MS      2000

#define STM_OTA_MAGIC           0xA55A
#define STM_OTA_TYPE_BEGIN      0x01
#define STM_OTA_TYPE_DATA       0x02
#define STM_OTA_TYPE_END        0x03
#define STM_OTA_TYPE_ABORT      0x04

#define MQTT_OTA_BEGIN          0x01
#define MQTT_OTA_DATA           0x02
#define MQTT_OTA_END            0x03

static const char *TAG = "ota";

static uint32_t s_seq = 0;
static uint32_t s_ota_total_size = 0;
static uint32_t s_ota_sent_size = 0;
static uint32_t s_mqtt_expected_seq = 0;
static bool s_ota_running = false;

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t type;
    uint32_t seq;
    uint16_t len;
    uint16_t crc16;
} ota_packet_header_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint32_t seq;
    uint32_t size;
    uint32_t crc32;
} ota_mqtt_header_t;

static uint16_t ota_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;

    if (data == NULL || len == 0) {
        return crc;
    }

    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];

        for (uint8_t bit = 0; bit < 8; bit++) {
            if ((crc & 1) != 0) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

static uint32_t ota_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;

    if (data == NULL || len == 0) {
        return 0;
    }

    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];

        for (uint8_t bit = 0; bit < 8; bit++) {
            if ((crc & 1) != 0) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }

    return ~crc;
}

static void ota_print_progress(void)
{
    uint32_t percent = 0;

    if (s_ota_total_size > 0) {
        percent = (s_ota_sent_size * 100U) / s_ota_total_size;
        if (percent > 100U) {
            percent = 100U;
        }
    }

    ESP_LOGI(TAG, "STM32 OTA progress: %lu/%lu bytes (%lu%%)",
             (unsigned long)s_ota_sent_size,
             (unsigned long)s_ota_total_size,
             (unsigned long)percent);
}

static bool stm32_wait_ack(uint32_t expect_seq, uint32_t timeout_ms)
{
    char rx[64] = {0};

    int len = uart_receive_timeout((uint8_t *)rx, sizeof(rx) - 1, timeout_ms);
    if (len <= 0) {
        ESP_LOGE(TAG, "ACK timeout seq=%lu", (unsigned long)expect_seq);
        return false;
    }

    rx[len] = '\0';

    char expect[32];
    snprintf(expect, sizeof(expect), "OK:%lu", (unsigned long)expect_seq);

    if (strstr(rx, expect) != NULL) {
        ESP_LOGI(TAG, "ACK %lu", (unsigned long)expect_seq);
        return true;
    }

    ESP_LOGE(TAG, "bad ACK: %s", rx);
    return false;
}

static bool stm32_send_packet(uint8_t type,
                              uint32_t seq,
                              const uint8_t *payload,
                              uint16_t len)
{
    ota_packet_header_t header = {
        .magic = STM_OTA_MAGIC,
        .type = type,
        .seq = seq,
        .len = len,
        .crc16 = ota_crc16(payload, len),
    };

    uart_send_bytes((const uint8_t *)&header, sizeof(header));

    if (payload != NULL && len > 0) {
        uart_send_bytes(payload, len);
    }

    return stm32_wait_ack(seq, STM_OTA_TIMEOUT_MS);
}

bool ota_stm32_begin(uint32_t firmware_size, uint32_t firmware_crc32)
{
    uint8_t payload[8];

    memcpy(&payload[0], &firmware_size, sizeof(firmware_size));
    memcpy(&payload[4], &firmware_crc32, sizeof(firmware_crc32));

    s_seq = 0;

    ESP_LOGI(TAG, "OTA begin size=%lu crc32=0x%08lx",
             (unsigned long)firmware_size,
             (unsigned long)firmware_crc32);

    return stm32_send_packet(STM_OTA_TYPE_BEGIN, s_seq++, payload, sizeof(payload));
}

bool ota_stm32_send_chunk(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0) {
        return false;
    }

    ESP_LOGI(TAG, "OTA data seq=%lu len=%u",
             (unsigned long)s_seq,
             len);

    return stm32_send_packet(STM_OTA_TYPE_DATA, s_seq++, data, len);
}

bool ota_stm32_end(void)
{
    ESP_LOGI(TAG, "OTA end");

    return stm32_send_packet(STM_OTA_TYPE_END, s_seq++, NULL, 0);
}

void ota_stm32_abort(void)
{
    ota_packet_header_t header = {
        .magic = STM_OTA_MAGIC,
        .type = STM_OTA_TYPE_ABORT,
        .seq = s_seq++,
        .len = 0,
        .crc16 = 0,
    };

    ESP_LOGW(TAG, "OTA abort");
    uart_send_bytes((const uint8_t *)&header, sizeof(header));
}

void ota_mqtt_handle_packet(const uint8_t *data, int len)
{
    if (data == NULL || len < (int)sizeof(ota_mqtt_header_t)) {
        ESP_LOGE(TAG, "MQTT OTA packet too short len=%d", len);
        return;
    }

    const ota_mqtt_header_t *hdr = (const ota_mqtt_header_t *)data;
    const uint8_t *payload = data + sizeof(ota_mqtt_header_t);
    int payload_len = len - (int)sizeof(ota_mqtt_header_t);

    switch (hdr->type) {
    case MQTT_OTA_BEGIN:
        ESP_LOGI(TAG, "MQTT OTA BEGIN seq=%lu size=%lu crc32=0x%08lx",
                 (unsigned long)hdr->seq,
                 (unsigned long)hdr->size,
                 (unsigned long)hdr->crc32);

        s_ota_total_size = hdr->size;
        s_ota_sent_size = 0;
        s_mqtt_expected_seq = hdr->seq + 1;
        s_ota_running = ota_stm32_begin(hdr->size, hdr->crc32);

        if (!s_ota_running) {
            ESP_LOGE(TAG, "STM32 OTA begin failed");
        }
        break;

    case MQTT_OTA_DATA:
        if (!s_ota_running) {
            ESP_LOGW(TAG, "ignore OTA DATA because OTA is not running");
            return;
        }

        if (hdr->seq != s_mqtt_expected_seq) {
            ESP_LOGW(TAG, "OTA sequence mismatch expected=%lu got=%lu",
                     (unsigned long)s_mqtt_expected_seq,
                     (unsigned long)hdr->seq);
            s_mqtt_expected_seq = hdr->seq;
        }

        if (hdr->size != (uint32_t)payload_len) {
            ESP_LOGE(TAG, "OTA chunk length mismatch header=%lu payload=%d",
                     (unsigned long)hdr->size,
                     payload_len);
            ota_stm32_abort();
            s_ota_running = false;
            return;
        }

        if (ota_crc32(payload, payload_len) != hdr->crc32) {
            ESP_LOGE(TAG, "OTA chunk crc32 mismatch seq=%lu", (unsigned long)hdr->seq);
            ota_stm32_abort();
            s_ota_running = false;
            return;
        }

        if (!ota_stm32_send_chunk(payload, (uint16_t)payload_len)) {
            ESP_LOGE(TAG, "send OTA chunk to STM32 failed seq=%lu", (unsigned long)hdr->seq);
            ota_stm32_abort();
            s_ota_running = false;
            return;
        }

        s_ota_sent_size += payload_len;
        s_mqtt_expected_seq = hdr->seq + 1;
        ota_print_progress();
        break;

    case MQTT_OTA_END:
        ESP_LOGI(TAG, "MQTT OTA END seq=%lu", (unsigned long)hdr->seq);

        if (!s_ota_running) {
            ESP_LOGW(TAG, "ignore OTA END because OTA is not running");
            return;
        }

        if (s_ota_sent_size != s_ota_total_size) {
            ESP_LOGE(TAG, "OTA size mismatch sent=%lu total=%lu",
                     (unsigned long)s_ota_sent_size,
                     (unsigned long)s_ota_total_size);
            ota_stm32_abort();
            s_ota_running = false;
            return;
        }

        if (ota_stm32_end()) {
            ESP_LOGI(TAG, "STM32 OTA done 100%%");
        } else {
            ESP_LOGE(TAG, "STM32 OTA end failed");
        }

        s_ota_running = false;
        break;

    default:
        ESP_LOGW(TAG, "unknown MQTT OTA packet type=%u", hdr->type);
        break;
    }
}

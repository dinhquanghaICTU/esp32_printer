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

static const char *TAG = "ota";

static uint32_t s_seq = 0;

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t type;
    uint32_t seq;
    uint16_t len;
    uint16_t crc16;
} ota_packet_header_t;

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

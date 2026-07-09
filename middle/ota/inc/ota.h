#ifndef __OTA_H__
#define __OTA_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

bool ota_stm32_begin(uint32_t firmware_size, uint32_t firmware_crc32);
bool ota_stm32_send_chunk(const uint8_t *data, uint16_t len);
bool ota_stm32_end(void);
void ota_stm32_abort(void);

#endif //__OTA_H__
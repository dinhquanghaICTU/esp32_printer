#ifndef __BLE_H__
#define __BLE_H__

#include <stdint.h>
#include <stdbool.h>

void ble_init(void);
void ble_notify(const uint8_t *data, uint16_t len);
const char *ble_get_ssid(void);
const char *ble_get_pass(void);

void ble_start_advertising(void);
void ble_stop_advertising(void);
bool ble_wifi_config_done(void);
void ble_clear_wifi_config_done(void);

#endif //__BLE_H__


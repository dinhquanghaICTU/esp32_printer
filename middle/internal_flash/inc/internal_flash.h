#ifndef __INTERNAL_FLASH_H__
#define __INTERNAL_FLASH_H__

#include <stddef.h>

void internal_flash_init(void);
void save_wifi_config(const char *ssid, const char *pass);
int load_wifi_config(char *ssid, size_t ssid_size, char *pass, size_t pass_size);
void erase_wifi_config(void);
#endif // __INTERNAL_FLASH_H__

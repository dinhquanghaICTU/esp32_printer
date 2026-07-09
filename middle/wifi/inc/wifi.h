#ifndef __WIFI_H__
#define __WIFI_H__

void wifi_init_sta(const char *ssid, const char *pass);
void wifi_stop_sta(void);

bool get_wifi_retry (void);
void clear_wifi_retry(void);
bool get_ip(void);
void clear_get_ip (void);

#endif // __WIFI_H__

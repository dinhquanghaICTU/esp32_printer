#include "internal_flash.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "stdint.h"

void internal_flash_init(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);
}

void save_wifi_config(const char *ssid, const char *pass)
{
    nvs_handle_t handle;

    ESP_ERROR_CHECK(nvs_open("wifi_cfg", NVS_READWRITE, &handle));

    ESP_ERROR_CHECK(nvs_set_str(handle, "ssid", ssid));
    ESP_ERROR_CHECK(nvs_set_str(handle, "pass", pass));

    ESP_ERROR_CHECK(nvs_commit(handle));

    nvs_close(handle);
}


int load_wifi_config(char *ssid, size_t ssid_size,
                     char *pass, size_t pass_size)
{
    nvs_handle_t handle;

    esp_err_t ret = nvs_open("wifi_cfg", NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        ssid[0] = '\0';
        pass[0] = '\0';
        return 0;
    }

    ret = nvs_get_str(handle, "ssid", ssid, &ssid_size);
    if (ret != ESP_OK || ssid[0] == '\0') {
        ssid[0] = '\0';
        pass[0] = '\0';
        nvs_close(handle);
        return 0;
    }

    ret = nvs_get_str(handle, "pass", pass, &pass_size);
    if (ret != ESP_OK) {
        pass[0] = '\0';
        nvs_close(handle);
        return 0;
    }

    nvs_close(handle);
    return 1;
}

void erase_wifi_config(void)
{
    nvs_handle_t handle;

    ESP_ERROR_CHECK(nvs_open("wifi_cfg", NVS_READWRITE, &handle));
    ESP_ERROR_CHECK(nvs_erase_all(handle));
    ESP_ERROR_CHECK(nvs_commit(handle));
    nvs_close(handle);
    printf("WiFi config erased.\n");
}
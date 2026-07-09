#include "button.h"
#include "ble.h"
#include "internal_flash.h"
#include "app.h"
#include "led.h"
#include "freertos/FreeRTOS.h"
#include "uart.h"
#include "ota.h"

void app_main(void)
{
    button_init();
    led_init();
    internal_flash_init();
    uart_init();
    ota_init();
    app_loop();
    // led_on();

    // while (1)
    // {
    //     // uint8_t buff[128];

    //     // int len = uart_receive_timeout(buff, sizeof(buff) - 1, 1000);

    //     // if (len > 0) {
    //     //     buff[len] = '\0';
    //     //     printf("STM32 -> ESP32: %s\r\n", buff);
    //     // } else {
    //     //     printf("no uart data\r\n");
    //     // }

    //     uart_send_string("hello stm32\r\n");

    //     vTaskDelay(pdMS_TO_TICKS(100));
    // }
}

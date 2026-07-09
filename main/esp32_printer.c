#include "button.h"
#include "ble.h"
#include "internal_flash.h"
#include "app.h"
#include "led.h"
#include "freertos/FreeRTOS.h"
#include "uart.h"

void app_main(void)
{
    button_init();
    led_init();
    internal_flash_init();
    uart_init();
    app_loop();
    // led_on();

    // while (1)
    // {
    //     led_blink();
    //     printf("loop\r\n");
    //     uart_send_string("quangha\r\n");
        
    //     vTaskDelay(pdMS_TO_TICKS(100));
    // }
}

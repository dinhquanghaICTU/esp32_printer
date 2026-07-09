#include "button.h"
#include "driver/gpio.h"
#include "stdio.h"
#include "freertos/FreeRTOS.h"


#define BUTTON_GPIO GPIO_NUM_0

void button_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

bool button_is_pressed(void)
{

    return gpio_get_level(BUTTON_GPIO) == 0;
}

int button_hold(void)
{

    int count = 0;
    while (gpio_get_level(BUTTON_GPIO) == 0)
    {
        count++;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    
    return count;
}
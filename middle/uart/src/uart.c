#include "uart.h"

#include <stdint.h>
#include <string.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ESP_UART_PORT UART_NUM_2
#define ESP_UART_TX   GPIO_NUM_17
#define ESP_UART_RX   GPIO_NUM_16
#define ESP_UART_BAUD 115200
#define ESP_UART_BUF  1024

void uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = ESP_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_param_config(ESP_UART_PORT, &uart_config);
    uart_set_pin(ESP_UART_PORT,
                 ESP_UART_TX,
                 ESP_UART_RX,
                 UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE);

    uart_driver_install(ESP_UART_PORT, ESP_UART_BUF * 2, ESP_UART_BUF * 2, 0, NULL, 0);
}

void uart_send(const char *data, size_t length)
{
    uart_write_bytes(ESP_UART_PORT, data, length);
}

void uart_send_string(const char *data)
{
    // printf("da send\r\n");
    if (data == NULL) {
        return;
    }

    uart_send(data, strlen(data));
}
void uart_send_bytes(const uint8_t *data, size_t length)
{
    uart_write_bytes(ESP_UART_PORT, data, length);
}

void uart_receive(char *buffer, size_t length)
{
    if (buffer == NULL || length == 0) {
        return;
    }

    int bytes_read = uart_read_bytes(ESP_UART_PORT,
                                     (uint8_t *)buffer,
                                     length - 1,
                                     portMAX_DELAY);
    if (bytes_read < 0) {
        bytes_read = 0;
    }

    buffer[bytes_read] = '\0';
}

int uart_receive_timeout(uint8_t *buffer, size_t length, uint32_t timeout_ms)
{
    if (buffer == NULL || length == 0) {
        return 0;
    }

    return uart_read_bytes(ESP_UART_PORT,
                           buffer,
                           length,
                           pdMS_TO_TICKS(timeout_ms));
}

void uart_flush_rx(void)
{
    uart_flush_input(ESP_UART_PORT);
}




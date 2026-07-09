#ifndef __UART_H__
#define __UART_H__

#include <stddef.h>
#include <stdint.h>

void uart_init(void);
void uart_send(const char *data, size_t length);
void uart_send_string(const char *data);
void uart_send_bytes(const uint8_t *data, size_t length);
void uart_receive(char *buffer, size_t length);
int uart_receive_timeout(uint8_t *buffer, size_t length, uint32_t timeout_ms);

#endif // __UART_H__


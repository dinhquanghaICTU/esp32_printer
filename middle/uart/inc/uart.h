#ifndef __UART_H__
#define __UART_H__

#include <stddef.h>

void uart_init(void);
void uart_send(const char *data, size_t length);
void uart_receive(char *buffer, size_t length);

#endif // __UART_H__

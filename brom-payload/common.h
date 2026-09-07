#pragma once

#include <inttypes.h>

extern int (*send_dword)();
extern int (*recv_dword)();
// addr, sz
extern int (*send_data)();
// addr, sz, flags (=0)
extern int (*recv_data)();

void low_uart_put(int ch);
void _putchar(char character);

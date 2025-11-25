#define NANOPRINTF_IMPLEMENTATION

#include "debug.h"

void low_uart_put(int ch) {
    while (!(*(volatile uint32_t*)0x11002014 & 0x20))
        ;
    *(volatile uint32_t*)0x11002000 = ch;
}

void uart_putc(int c, void* ctx) {
    (void)ctx;
    if (c == '\n') low_uart_put('\r');
    low_uart_put(c);
}

int video_printf(const char* fmt, ...) {
    return ((int (*)(const char*))(0x46032DAC | 1))(fmt);
}

int printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int ret = npf_vpprintf(&uart_putc, NULL, fmt, args);

    va_end(args);
    return ret;
}

void hex_dump(const void* data, size_t size) {
    size_t i, j;
    for (i = 0; i < size; ++i) {
        printf("%02X ", ((unsigned char*)data)[i]);
        if ((i+1) % 8 == 0 || i+1 == size) {
            printf(" ");
            if ((i+1) % 16 == 0) {
                printf("\n");
            } else if (i+1 == size) {
                if ((i+1) % 16 <= 8) {
                    printf(" ");
                }
                for (j = (i+1) % 16; j < 16; ++j) {
                    printf("   ");
                }
                printf("\n");
            }
        }
    }
}
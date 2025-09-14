#include <inttypes.h>

#include "libc.h"
#include "common.h"

void low_uart_put(int ch) {
    volatile uint32_t *uart_reg0 = (volatile uint32_t*)0x11002014;
    volatile uint32_t *uart_reg1 = (volatile uint32_t*)0x11002000;

    while ( !((*uart_reg0) & 0x20) )
    {}

    *uart_reg1 = ch;
}

void _putchar(char character)
{
    if (character == '\n')
        low_uart_put('\r');
    low_uart_put(character);
}

uint64_t g_boot, g_recovery, g_lk, g_misc;

int (*original_read)(part_dev_t *dev, uint64_t block_off, void *dst, size_t sz, int part) = (void*)(0x460533CD|1);

int read_func(part_dev_t *dev, uint64_t block_off, void *dst, size_t sz, int part) {
    printf("read_func hook\n");
    int ret = 0;
    if (block_off == g_boot * 0x200 || block_off == g_recovery * 0x200) {
        printf("demangle boot image - from 0x%08X\n", __builtin_return_address(0));

        if (sz < 0x400) {
            ret = original_read(dev, block_off + 0x400, dst, sz, part);
        } else {
            void *second_copy = (char*)dst + 0x400;
            ret = original_read(dev, block_off, dst, sz, part);
            memcpy(dst, second_copy, 0x400);
            memset(second_copy, 0, 0x400);
        }
    } else {
        ret = original_read(dev, block_off, dst, sz, part);
    }
    return ret;
}

static void parse_gpt(part_dev_t *dev) {
    uint8_t raw[0x800] = { 0 };
    dev->read(dev, 0x400, raw, sizeof(raw), USER_PART);
    for (int i = 0; i < sizeof(raw) / 0x80; ++i) {
        uint8_t *ptr = &raw[i * 0x80];
        uint8_t *name = ptr + 0x38;
        uint32_t start;
        memcpy(&start, ptr + 0x20, 4);
        if (memcmp(name, "b\x00o\x00o\x00t\x00\x00\x00", 10) == 0) {
            printf("found boot at 0x%08X\n", start);
            g_boot = start;
        } else if (memcmp(name, "r\x00\x65\x00\x63\x00o\x00v\x00\x65\x00r\x00y\x00\x00\x00", 18) == 0) {
            printf("found recovery at 0x%08X\n", start);
            g_recovery = start;
        } else if (memcmp(name, "l\x00k\x00\x00\x00", 6) == 0) {
            printf("found lk at 0x%08X\n", start);
            g_lk = start;
        } else if (memcmp(name, "M\x00I\x00S\x00\x43\x00\x00\x00", 10) == 0) {
            printf("found misc at 0x%08X\n", start);
            g_misc = start;
        }
    }
}

int main() {
    int ret = 0, fastboot = 0;
    uint16_t *patch;
    uint32_t *patch32;

    printf("This is LK-payload by xyz. Copyright 2019\n");
    printf("Updated version by k4y0z. Copyright 2019\n");
    printf("Ported to LG K10 by R0rt1z2. Copyright 2025\n");

    part_dev_t *dev = get_device();
    parse_gpt(dev);

    if (!g_boot || !g_recovery || !g_lk) {
        printf("failed to find boot, recovery or lk\n");
        printf("falling back to fastboot mode\n");
        fastboot = 1;
    }

    original_read = (void*)dev->read;
    dev->read = (void*)read_func;

    if (fastboot) {
	    *g_boot_mode = 99;
      patch = (uint16_t*)0x4602A3B8;
      patch[0] = 0x46c0; // nop
      patch[1] = 0x46c0; // nop

      printf("Well since you're asking so nicely...\n");
      video_printf(" => HACKED FASTBOOT mode...\n");
    }

    arch_clean_invalidate_cache_range(LK_BASE, LK_SIZE);

    printf("jump to lk at 0x%08X\n", (unsigned int)app);
    app();

    while (1) {

    }
}
